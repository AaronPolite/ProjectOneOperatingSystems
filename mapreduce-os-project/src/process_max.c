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
#include <limits.h>
#include <semaphore.h>

// Structure to define a work unit for each process
typedef struct { size_t startIdx; size_t len; } WorkUnit;

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

    size_t baseChunk = N / requestedWorkers, remainder = N % requestedWorkers;
    WorkUnit *jobs = (WorkUnit*)malloc(requestedWorkers*sizeof(WorkUnit)); if (!jobs){ perror("malloc jobs"); return 1; }
    size_t offset=0, jobCount=0;
    for (int w=0; w<requestedWorkers; w++){
        size_t L = baseChunk + ((size_t)w < remainder ? 1 : 0);
        if (L > 0) { jobs[jobCount].startIdx = offset; jobs[jobCount].len = L; jobCount++; offset += L; }
    }

    char shmName[64]; snprintf(shmName, sizeof(shmName), "/max_shm_%d", getpid());
    int shmFd = shm_open(shmName, O_CREAT|O_RDWR, 0600); if (shmFd < 0){ perror("shm_open"); return 1; }
    if (ftruncate(shmFd, sizeof(int)) != 0){ perror("ftruncate"); return 1; }
    int *sharedMax = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (sharedMax == MAP_FAILED){ perror("mmap"); return 1; }
    *sharedMax = INT_MIN;

    char semName[64]; snprintf(semName, sizeof(semName), "/max_sem_%d", getpid());
    sem_t *lock = sem_open(semName, O_CREAT, 0600, 1);
    if (lock == SEM_FAILED){ perror("sem_open"); return 1; }

    double t0 = nowMs();
    pid_t *pids = (pid_t*)malloc(jobCount*sizeof(pid_t)); if (!pids){ perror("malloc pids"); return 1; }

    // Create processes to find local maxima
    for (size_t i=0;i<jobCount;i++){
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        else if (pid == 0) {
            size_t start = jobs[i].startIdx, len = jobs[i].len;
            if (len > 0) {
                int localMax = data[start];
                for (size_t k=1;k<len;k++){ int v = data[start + k]; if (v > localMax) localMax = v; }
                sem_wait(lock);
                if (localMax > *sharedMax) *sharedMax = localMax;
                sem_post(lock);
            }
            _exit(0);
        } else {
            pids[i] = pid;
        }
    }
    // Wait for all child processes to finish
    for (size_t i=0;i<jobCount;i++){ int status; waitpid(pids[i], &status, 0); }

    double t1 = nowMs();
    long memKB = getMemoryUsageKB();
    printf("=== process_max results ===\n");
    printf("Array size: %zu\n", N);
    printf("Workers (processes): %zu\n", jobCount);
    printf("Global max: %d\n", *sharedMax);
    printf("Time: %.3f ms\n", (t1 - t0));
    printf("Approx Parent RSS: %ld KB\n", memKB);

    if (N <= 32) {
        printf("Input array:\n[");
        for (size_t i = 0; i < N; i++) {
            printf("%d", data[i]);
            if (i + 1 < N) printf(", ");
        }
        printf("]\n");
    }

    free(pids); free(jobs); free(data);
    munmap(sharedMax, sizeof(int)); close(shmFd); shm_unlink(shmName);
    sem_close(lock); sem_unlink(semName);
    return 0;
}
