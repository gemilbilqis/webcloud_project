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
#define BLOCK_SIZE 16

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

void process_block(const unsigned char* in, unsigned char* out, int len, EVP_CIPHER_CTX* shared_ctx, int decrypt) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_copy(ctx, shared_ctx);

    int len1, len2;
    if (!decrypt) {
        EVP_EncryptUpdate(ctx, out, &len1, in, len);
        EVP_EncryptFinal_ex(ctx, out + len1, &len2);
    } else {
        EVP_DecryptUpdate(ctx, out, &len1, in, len);
        EVP_DecryptFinal_ex(ctx, out + len1, &len2);
    }

    EVP_CIPHER_CTX_free(ctx);
}

unsigned char* read_file(const char* filename, long* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) { perror("fopen"); exit(1); }
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);
    unsigned char* buffer = malloc(*size);
    if (!buffer) { perror("malloc"); exit(1); }
    fread(buffer, 1, *size, file);
    fclose(file);
    return buffer;
}

void write_file(const char* filename, const unsigned char* data, long size) {
    FILE* file = fopen(filename, "wb");
    if (!file) { perror("fopen"); exit(1); }
    fwrite(data, 1, size, file);
    fclose(file);
}

int main(int argc, char* argv[]) {
    printf("Received %d arguments:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    if (argc != 7) {
        fprintf(stderr, "Usage: %s input.bmp output.bmp encrypt|decrypt ECB|CTR base64key base64iv\n", argv[0]);
        return 1;
    }
    const char* in_file = argv[1];
    const char* out_file = argv[2];
    int decrypt = strcmp(argv[3], "decrypt") == 0;
    const char* mode = argv[4];
    const char* base64key = argv[5];
    const char* base64iv = argv[6];

    unsigned char *key = NULL, *iv = NULL;
    int key_len = base64_decode(base64key, &key);
    int iv_len  = base64_decode(base64iv, &iv);

    if (key_len != 16 || iv_len != 16) {
        fprintf(stderr, "Invalid AES key/IV length\n");
        exit(1);
    }

    MPI_Init(&argc, &argv);
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    long file_size;
    unsigned char* input = read_file(in_file, &file_size);
    if (file_size < HEADER_SIZE) { fprintf(stderr, "File too small\n"); exit(1); }

    unsigned char* header = input;
    unsigned char* data = input + HEADER_SIZE;
    long data_len = file_size - HEADER_SIZE;

    long chunk_size = data_len / world_size;
    long offset = rank * chunk_size;
    if (rank == world_size - 1) chunk_size += data_len % world_size;

    unsigned char* processed_chunk = malloc(chunk_size + EVP_MAX_BLOCK_LENGTH);

    const EVP_CIPHER* cipher = NULL;
    if (strcmp(mode, "ECB") == 0) cipher = EVP_aes_128_ecb();
    else if (strcmp(mode, "CTR") == 0) cipher = EVP_aes_128_ctr();
    else { fprintf(stderr, "Unsupported mode\n"); exit(1); }

    EVP_CIPHER_CTX* shared_ctx = EVP_CIPHER_CTX_new();
    if (!decrypt)
        EVP_EncryptInit_ex(shared_ctx, cipher, NULL, key, iv);
    else
        EVP_DecryptInit_ex(shared_ctx, cipher, NULL, key, iv);
    EVP_CIPHER_CTX_set_padding(shared_ctx, 0);

    long aligned_chunk = chunk_size - (chunk_size % BLOCK_SIZE);

    #pragma omp parallel for schedule(static)
    for (long i = 0; i < aligned_chunk; i += BLOCK_SIZE) {
        process_block(data + offset + i, processed_chunk + i, BLOCK_SIZE, shared_ctx, decrypt);
    }

    // Copy any remaining bytes unprocessed (edge case)
    if (rank == world_size - 1 && chunk_size % BLOCK_SIZE != 0) {
        long remainder = chunk_size % BLOCK_SIZE;
        memcpy(processed_chunk + aligned_chunk, data + offset + aligned_chunk, remainder);
    }


    EVP_CIPHER_CTX_free(shared_ctx);

    unsigned char* all_processed = NULL;
    int* recvcounts = NULL;
    int* displs = NULL;

    if (rank == 0) {
        all_processed = malloc(data_len);
        recvcounts = malloc(world_size * sizeof(int));
        displs = malloc(world_size * sizeof(int));
        for (int i = 0; i < world_size; i++) {
            recvcounts[i] = data_len / world_size;
            if (i == world_size - 1) recvcounts[i] += data_len % world_size;
            displs[i] = (data_len / world_size) * i;
        }
    }

    MPI_Gatherv(processed_chunk, chunk_size, MPI_BYTE,
                all_processed, recvcounts, displs, MPI_BYTE,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned char* output = malloc(file_size);
        memcpy(output, header, HEADER_SIZE);
        memcpy(output + HEADER_SIZE, all_processed, data_len);
        write_file(out_file, output, file_size);
        printf("%s BMP written to %s\n", decrypt ? "Decrypted" : "Encrypted", out_file);
        free(output);
        free(all_processed);
        free(recvcounts);
        free(displs);
    }

    free(processed_chunk);
    free(input);
    free(key);
    free(iv);

    MPI_Finalize();
    return 0;
}
