#!/usr/bin/env python3
# Verify if the input array is sorted
import sys

def is_sorted(nums):
    return all(nums[i] <= nums[i+1] for i in range(len(nums)-1))

def main():
    data = []
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        if line.startswith('[') and line.endswith(']'):
            line = line[1:-1]
            if line.strip()=='':
                print("OK: empty list")
                return 0
            parts = [p.strip() for p in line.split(',')]
            try:
                data = list(map(int, parts))
            except ValueError:
                print("ERROR: could not parse ints")
                return 1
            break
    if not data:
        print("ERROR: no array found (expecting [1, 2, ...])")
        return 1
    print("OK" if is_sorted(data) else "NOT SORTED")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
