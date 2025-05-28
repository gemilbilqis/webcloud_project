#include <mpi.h>
#include <omp.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 54

// Decode base64 to raw bytes
int base64_decode(const char* base64, unsigned char** buffer) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_mem_buf(base64, -1);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    size_t length = strlen(base64);
    *buffer = (unsigned char*)malloc(length);
    int decoded = BIO_read(bio, *buffer, length);
    BIO_free_all(bio);
    return decoded;
}

// Encrypt a block in-place using OpenSSL
void encrypt_block(unsigned char* in, unsigned char* out, int len, EVP_CIPHER_CTX* ctx) {
    int out_len1, out_len2;
    EVP_EncryptUpdate(ctx, out, &out_len1, in, len);
    EVP_EncryptFinal_ex(ctx, out + out_len1, &out_len2);
}

// Read entire file into buffer
unsigned char* read_file(const char* filename, long* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) { perror("fopen"); exit(1); }
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);
    unsigned char* buffer = malloc(*size);
    fread(buffer, 1, *size, file);
    fclose(file);
    return buffer;
}

// Write buffer to file
void write_file(const char* filename, unsigned char* data, long size) {
    FILE* file = fopen(filename, "wb");
    if (!file) { perror("fopen"); exit(1); }
    fwrite(data, 1, size, file);
    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s input.bmp output.bmp ECB|CTR base64key base64iv\n", argv[0]);
        return 1;
    }

    const char* in_file = argv[1];
    const char* out_file = argv[2];
    const char* mode = argv[3];
    const char* base64key = argv[4];
    const char* base64iv = argv[5];

    // Decode AES key and IV
    unsigned char *key, *iv;
    int key_len = base64_decode(base64key, &key);
    int iv_len = base64_decode(base64iv, &iv);

    // Init OpenMPI
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long file_size;
    unsigned char* input = read_file(in_file, &file_size);
    if (file_size < HEADER_SIZE) { fprintf(stderr, "File too small\n"); exit(1); }

    unsigned char* header = input;
    unsigned char* data = input + HEADER_SIZE;
    long data_len = file_size - HEADER_SIZE;

    // Split data among MPI processes
    long chunk_size = data_len / size;
    long offset = rank * chunk_size;
    if (rank == size - 1) { chunk_size += data_len % size; }

    unsigned char* encrypted_chunk = malloc(chunk_size + EVP_MAX_BLOCK_LENGTH);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    const EVP_CIPHER* cipher = NULL;

    if (strcmp(mode, "ECB") == 0) cipher = EVP_aes_128_ecb();
    else if (strcmp(mode, "CTR") == 0) cipher = EVP_aes_128_ctr();
    else { fprintf(stderr, "Unsupported mode\n"); exit(1); }

    EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv);
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    // OpenMP parallel encryption
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < chunk_size; i += 16) {
        unsigned char out[32];
        encrypt_block(data + offset + i, encrypted_chunk + i, 16, ctx);
    }

    // Gather encrypted data to master
    unsigned char* output_data = NULL;
    if (rank == 0) output_data = malloc(data_len);

    int* recvcounts = NULL;
    int* displs = NULL;
    if (rank == 0) {
        recvcounts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));
        for (int i = 0; i < size; i++) {
            recvcounts[i] = data_len / size;
            if (i == size - 1) recvcounts[i] += data_len % size;
            displs[i] = (data_len / size) * i;
        }
    }

    MPI_Gatherv(encrypted_chunk, chunk_size, MPI_BYTE,
                output_data, recvcounts, displs, MPI_BYTE,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned char* full_output = malloc(file_size);
        memcpy(full_output, header, HEADER_SIZE);
        memcpy(full_output + HEADER_SIZE, output_data, data_len);
        write_file(out_file, full_output, file_size);
        printf("✅ Encrypted BMP written to %s\n", out_file);
        free(full_output);
        free(output_data);
        free(recvcounts);
        free(displs);
    }

    free(encrypted_chunk);
    free(input);
    EVP_CIPHER_CTX_free(ctx);
    free(key); free(iv);

    MPI_Finalize();
    return 0;
}
