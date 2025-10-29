#!/usr/bin/env bash
set -euo pipefail
make
echo "== Small correctness runs (N=32, 4 workers) =="
./bin/thread_sort   32 4 | tee results/small/thread_sort.txt
./bin/process_sort  32 4 | tee results/small/process_sort.txt
./bin/thread_max    32 4 | tee results/small/thread_max.txt
./bin/process_max   32 4 | tee results/small/process_max.txt
echo "Outputs saved under results/small/"
