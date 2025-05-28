#include <mpi.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    MPI_Init(NULL, NULL);
    int rank, size;
    char hostname[1024];
    gethostname(hostname, 1024);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    printf("Hello from rank %d of %d on host %s\n", rank, size, hostname);
    MPI_Finalize();
    return 0;
}
