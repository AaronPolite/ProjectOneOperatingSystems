# MapReduce OS Project (Single-Host Simulation)

This repo contains four C programs that simulate MapReduce-style workloads on a single machine using **multithreading** and **multiprocessing** to explore OS concepts (process/thread management, IPC, synchronization).

## Programs

- `thread_sort.c` — Part 1, multithreading: map (sort chunks) + reduce (merge).
- `process_sort.c` — Part 1, multiprocessing + shared memory IPC.
- `thread_max.c` — Part 2, multithreading: single-int `globalMax` + mutex.
- `process_max.c` — Part 2, multiprocessing: single-int shared memory + semaphore.

## Build

```bash
make
```

## Run

Small (correctness) examples:
```bash
./bin/thread_sort   32 4
./bin/process_sort  32 4
./bin/thread_max    32 4
./bin/process_max   32 4
```

Large (performance) examples:
```bash
./bin/thread_sort   131072 8
./bin/process_sort  131072 8
./bin/thread_max    131072 8
./bin/process_max   131072 8
```

Or run helper scripts:
```bash
bash scripts/run_all_small.sh
bash scripts/run_all_large.sh
```
