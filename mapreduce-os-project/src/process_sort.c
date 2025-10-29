#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
// Structure to hold information about each chunk
typedef struct { int *base; size_t len; } ChunkInfo;

// Comparison function for qsort
static int intCompare(const void *a, const void *b){
    int ia = *(const int*)a, ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

// Merge two sorted arrays A and B into dest
static void mergeTwo(const int *A, size_t lenA, const int *B, size_t lenB, int *dest){
    size_t i = 0, j = 0, k = 0;
    while (i < lenA && j < lenB) {
        if (A[i] <= B[j]) dest[k++] = A[i++];
        else               dest[k++] = B[j++];
    }
    while (i < lenA) dest[k++] = A[i++];
    while (j < lenB) dest[k++] = B[j++];
}

// Merge all sorted chunks into a single sorted array
static void mergeAll(ChunkInfo *chunks, size_t chunkCount, int **outPtr, size_t *outLen){
    if (!chunkCount) { *outPtr = NULL; *outLen = 0; return; }
    int *merged = (int*)malloc(chunks[0].len * sizeof(int));
    if (!merged) { perror("malloc"); exit(1); }
    memcpy(merged, chunks[0].base, chunks[0].len * sizeof(int));
    size_t mergedLen = chunks[0].len;

    // Iteratively merge each chunk into the merged array
    for (size_t c = 1; c < chunkCount; c++) {
        size_t newLen = mergedLen + chunks[c].len;
        int *tmp = (int*)malloc(newLen * sizeof(int));
        if (!tmp) { perror("malloc"); exit(1); }
        mergeTwo(merged, mergedLen, chunks[c].base, chunks[c].len, tmp);
        free(merged);
        merged = tmp;
        mergedLen = newLen;
    }
    *outPtr = merged; *outLen = mergedLen;
}

// Function to get the current time in milliseconds
static double nowMs(){
    struct timeval tv; gettimeofday(&tv, NULL);
    return (double)tv.tv_sec*1000.0 + (double)tv.tv_usec/1000.0;
}

// Function to get the current memory usage in KB
static long getMemoryUsageKB(){
    FILE *f = fopen("/proc/self/statm","r"); if (!f) return -1;
    long t=0, r=0; if (fscanf(f,"%ld %ld",&t,&r)!=2) { fclose(f); return -1; }
    fclose(f);
    long ps = sysconf(_SC_PAGESIZE);
    return (r * ps) / 1024;
}

// Function to print an array of integers
static void printArray(const int *arr, size_t n){
    printf("[");
    for (size_t i=0;i<n;i++){ printf("%d",arr[i]); if (i+1<n) printf(", "); }
    printf("]\n");
}

// Main function
int main(int argc, char **argv){
    // Parse command line arguments
    if (argc != 3) {
        fprintf(stderr,"Usage: %s <arraySize> <numWorkers>\n", argv[0]);
        return 1;
    }
    size_t N = strtoull(argv[1], NULL, 10);
    int requestedWorkers = atoi(argv[2]);
    if (requestedWorkers < 1) requestedWorkers = 1;

    // Create shared memory for the array
    char shmName[64]; snprintf(shmName, sizeof(shmName), "/sort_shm_%d", getpid());
    int shmFd = shm_open(shmName, O_CREAT|O_RDWR, 0600); if (shmFd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(shmFd, N*sizeof(int)) != 0) { perror("ftruncate"); return 1; }
    int *sharedData = mmap(NULL, N*sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (sharedData == MAP_FAILED) { perror("mmap"); return 1; }

    srand(42);
    for (size_t i=0;i<N;i++) sharedData[i] = rand();
    
    // Divide the array into chunks for each process
    size_t baseChunk = N / requestedWorkers, remainder = N % requestedWorkers;
    ChunkInfo *chunks = (ChunkInfo*)malloc(requestedWorkers*sizeof(ChunkInfo));
    if (!chunks) { perror("malloc chunks"); return 1; }

    size_t offset=0, chunkCount=0;
    
    // Assign chunks to each worker
    for (int w=0; w<requestedWorkers; w++){
        size_t L = baseChunk + ((size_t)w < remainder ? 1 : 0);
        if (L > 0) {
            chunks[chunkCount].base = &sharedData[offset];
            chunks[chunkCount].len  = L;
            chunkCount++; offset += L;
        }
    }

    double t0 = nowMs();
    pid_t *pids = (pid_t*)malloc(chunkCount*sizeof(pid_t)); if (!pids){ perror("malloc pids"); return 1; }

    // Create processes to sort each chunk
    for (size_t i=0;i<chunkCount;i++){
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        else if (pid == 0) {
            if (chunks[i].len > 1) qsort(chunks[i].base, chunks[i].len, sizeof(int), intCompare);
            _exit(0);
        } else {
            pids[i] = pid;
        }
    }
    // Wait for all child processes to finish
    for (size_t i=0;i<chunkCount;i++){ int status; waitpid(pids[i], &status, 0); }

    int *finalSorted=NULL; size_t finalLen=0;
    mergeAll(chunks, chunkCount, &finalSorted, &finalLen);

    double t1 = nowMs();
    long memKB = getMemoryUsageKB();
    printf("=== process_sort results ===\n");
    printf("Array size: %zu\n", N);
    printf("Workers (processes): %zu\n", chunkCount);
    printf("Time: %.3f ms\n", (t1 - t0));
    printf("Approx Parent RSS: %ld KB\n", memKB);

    // Print the final sorted array if small enough
    if (N <= 32 && finalSorted) {
        printf("Final sorted array:\n");
        printArray(finalSorted, finalLen);
    }

    free(finalSorted); free(pids); free(chunks);
    munmap(sharedData, N*sizeof(int)); close(shmFd); shm_unlink(shmName);
    return 0;
}
