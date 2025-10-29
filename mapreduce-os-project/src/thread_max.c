#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <limits.h>

// Structure to define a range for each thread
typedef struct { int *base; size_t len; } Range;

static int globalMax;
static pthread_mutex_t maxLock = PTHREAD_MUTEX_INITIALIZER;

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

// Worker function for each thread to find local max
static void *workerFindMax(void *arg){
    Range *r = (Range*)arg; if (r->len == 0) return NULL;
    int localMax = r->base[0];
    for (size_t i=1;i<r->len;i++) if (r->base[i] > localMax) localMax = r->base[i];
    pthread_mutex_lock(&maxLock);
    if (localMax > globalMax) globalMax = localMax;
    pthread_mutex_unlock(&maxLock);
    return NULL;
}

// Main function
int main(int argc, char **argv){
    // Parse command line arguments
    if (argc != 3) {
        fprintf(stderr,"Usage: %s <arraySize> <numWorkers>\n", argv[0]);
        return 1;
    }
    size_t N = strtoull(argv[1], NULL, 10);
    int requestedWorkers = atoi(argv[2]); if (requestedWorkers < 1) requestedWorkers = 1;

    int *data = (int*)malloc(N*sizeof(int)); if (!data){ perror("malloc data"); return 1; }
    srand(42); for (size_t i=0;i<N;i++) data[i] = rand();

    // Divide the array into ranges for each thread
    size_t baseChunk = N / requestedWorkers, remainder = N % requestedWorkers;
    Range *ranges = (Range*)malloc(requestedWorkers*sizeof(Range)); if (!ranges){ perror("malloc ranges"); return 1; }
    size_t offset=0, chunkCount=0;
    
    // Assign ranges to each thread
    for (int w=0; w<requestedWorkers; w++){
        size_t L = baseChunk + ((size_t)w < remainder ? 1 : 0);
        if (L > 0) { ranges[chunkCount].base = &data[offset]; ranges[chunkCount].len = L; chunkCount++; offset += L; }
    }

    pthread_t *threads = (pthread_t*)malloc(chunkCount*sizeof(pthread_t)); if (!threads){ perror("malloc threads"); return 1; }
    globalMax = INT_MIN;

    double t0 = nowMs();
    // Launch threads
    for (size_t i=0;i<chunkCount;i++){
        if (pthread_create(&threads[i], NULL, workerFindMax, &ranges[i]) != 0) { perror("pthread_create"); return 1; }
    }
    for (size_t i=0;i<chunkCount;i++) pthread_join(threads[i], NULL);
    double t1 = nowMs();

    // Output results
    long memKB = getMemoryUsageKB();
    printf("=== thread_max results ===\n");
    printf("Array size: %zu\n", N);
    printf("Workers (threads): %zu\n", chunkCount);
    printf("Global max: %d\n", globalMax);
    printf("Time: %.3f ms\n", (t1 - t0));
    printf("Approx RSS: %ld KB\n", memKB);

    // Print the input array if small enough
    if (N <= 32) {
        printf("Input array:\n[");
        for (size_t i = 0; i < N; i++) {
            printf("%d", data[i]);
            if (i + 1 < N) printf(", ");
        }
        printf("]\n");
    }

    free(threads); free(ranges); free(data);
    return 0;
}
