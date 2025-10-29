#!/usr/bin/env bash
set -euo pipefail
make
N=131072
for W in 1 2 4 8; do
  echo "== thread_sort N=$N W=$W =="
  ./bin/thread_sort $N $W | tee -a results/large/thread_sort_${N}.log
done
for W in 1 2 4 8; do
  echo "== process_sort N=$N W=$W =="
  ./bin/process_sort $N $W | tee -a results/large/process_sort_${N}.log
done
for W in 1 2 4 8; do
  echo "== thread_max N=$N W=$W =="
  ./bin/thread_max $N $W | tee -a results/large/thread_max_${N}.log
done
for W in 1 2 4 8; do
  echo "== process_max N=$N W=$W =="
  ./bin/process_max $N $W | tee -a results/large/process_max_${N}.log
done
echo "Logs saved under results/large/"
