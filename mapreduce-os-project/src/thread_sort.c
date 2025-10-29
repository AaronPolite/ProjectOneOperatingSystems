/* ... (full code inserted) ... */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <unistd.h>

// Structure to hold information about each chunk
typedef struct {
    int *base;
    size_t len;
} ThreadArg;

// Comparison function for qsort
static int intCompare(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

// Worker function for each thread to sort its chunk
static void *threadSort(void *arg) {
    ThreadArg *t = (ThreadArg *)arg;
    if (t->len > 1) {
        qsort(t->base, t->len, sizeof(int), intCompare);
    }
    return NULL;
}

// Merge two sorted arrays A and B into dest
static void mergeTwo(const int *A, size_t lenA,
                     const int *B, size_t lenB,
                     int *dest) {
    size_t i = 0, j = 0, k = 0;
    while (i < lenA && j < lenB) {
        if (A[i] <= B[j]) dest[k++] = A[i++];
        else dest[k++] = B[j++];
    }
    while (i < lenA) dest[k++] = A[i++];
    while (j < lenB) dest[k++] = B[j++];
}

// Merge all sorted chunks into a single sorted array
static void mergeAll(ThreadArg *chunks, size_t chunkCount,
                     int **outPtr, size_t *outLen) {
    if (chunkCount == 0) { *outPtr=NULL; *outLen=0; return; }
    int *merged = (int *)malloc(chunks[0].len * sizeof(int));
    if (!merged) { perror("malloc"); exit(1); }
    memcpy(merged, chunks[0].base, chunks[0].len * sizeof(int));
    size_t mergedLen = chunks[0].len;
    for (size_t c = 1; c < chunkCount; c++) {
        size_t newLen = mergedLen + chunks[c].len;
        int *tmp = (int *)malloc(newLen * sizeof(int));
        if (!tmp) { perror("malloc"); exit(1); }
        mergeTwo(merged, mergedLen, chunks[c].base, chunks[c].len, tmp);
        free(merged);
        merged = tmp;
        mergedLen = newLen;
    }
    *outPtr = merged;
    *outLen = mergedLen;
}


// Function to get the current time in milliseconds
static double nowMs() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

// Function to get the current memory usage in KB
static long getMemoryUsageKB() {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long totalPages = 0, rssPages = 0;
    if (fscanf(f, "%ld %ld", &totalPages, &rssPages) != 2) { fclose(f); return -1; }
    fclose(f);
    long pageSize = sysconf(_SC_PAGESIZE);
    return (rssPages * pageSize) / 1024;
}

// Function to print an array of integers
static void printArray(const int *arr, size_t n) {
    printf("[");
    for (size_t i = 0; i < n; i++) { printf("%d", arr[i]); if (i + 1 < n) printf(", "); }
    printf("]\n");
}

// Main function
int main(int argc, char **argv) {
    // Parse command line arguments
    if (argc != 3) { fprintf(stderr,"Usage: %s <arraySize> <numWorkers>\n", argv[0]); return 1; }
    size_t N = strtoull(argv[1], NULL, 10);
    int requestedWorkers = atoi(argv[2]); if (requestedWorkers < 1) requestedWorkers = 1;

    int *data = (int *)malloc(N * sizeof(int));
    if (!data) { perror("malloc data"); return 1; }
    srand(42);
    for (size_t i = 0; i < N; i++) data[i] = rand();

    // Divide the array into chunks for each thread
    size_t baseChunk = N / requestedWorkers, remainder = N % requestedWorkers;
    ThreadArg *chunks = (ThreadArg *)malloc(requestedWorkers * sizeof(ThreadArg));
    if (!chunks) { perror("malloc chunks"); return 1; }

    size_t offset = 0, chunkCount = 0;
    // Assign chunks to each thread
    for (int w = 0; w < requestedWorkers; w++) {
        size_t thisLen = baseChunk + ((size_t)w < remainder ? 1 : 0);
        if (thisLen > 0) {
            chunks[chunkCount].base = &data[offset];
            chunks[chunkCount].len  = thisLen;
            chunkCount++; offset += thisLen;
        }
    }

    // Create and launch threads
    pthread_t *threads = (pthread_t *)malloc(chunkCount * sizeof(pthread_t));
    if (!threads) { perror("malloc threads"); return 1; }

    double t0 = nowMs();
    // Launch threads
    for (size_t i = 0; i < chunkCount; i++) {
        if (pthread_create(&threads[i], NULL, threadSort, &chunks[i]) != 0) { perror("pthread_create"); return 1; }
    }
    for (size_t i = 0; i < chunkCount; i++) pthread_join(threads[i], NULL);

    int *finalSorted=NULL; size_t finalLen=0;
    mergeAll(chunks, chunkCount, &finalSorted, &finalLen);
    double t1 = nowMs();

    // Output results
    long memKB = getMemoryUsageKB();
    printf("=== thread_sort results ===\n");
    printf("Array size: %zu\n", N);
    printf("Workers (threads): %zu\n", chunkCount);
    printf("Time: %.3f ms\n", (t1 - t0));
    printf("Approx RSS: %ld KB\n", memKB);
    if (N <= 32 && finalSorted) { printf("Final sorted array:\n"); printArray(finalSorted, finalLen); }

    free(finalSorted); free(threads); free(chunks); free(data);
    return 0;
}
