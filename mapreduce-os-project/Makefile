CC=gcc
CFLAGS=-O2 -Wall -Wextra
PTHREAD=-pthread
RT=-lrt

BIN_DIR=bin
SRC_DIR=src

ALL_BINS=$(BIN_DIR)/thread_sort $(BIN_DIR)/process_sort $(BIN_DIR)/thread_max $(BIN_DIR)/process_max

all: dirs $(ALL_BINS)

dirs:
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/thread_sort: $(SRC_DIR)/thread_sort.c
	$(CC) $(CFLAGS) $(PTHREAD) $< -o $@

$(BIN_DIR)/process_sort: $(SRC_DIR)/process_sort.c
	$(CC) $(CFLAGS) $< -o $@ $(RT)

$(BIN_DIR)/thread_max: $(SRC_DIR)/thread_max.c
	$(CC) $(CFLAGS) $(PTHREAD) $< -o $@

$(BIN_DIR)/process_max: $(SRC_DIR)/process_max.c
	$(CC) $(CFLAGS) $(PTHREAD) $< -o $@ $(RT)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all dirs clean
