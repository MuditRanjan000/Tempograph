# Makefile - TempoGraph Simulation Framework
# CSD 204 - Operating Systems Project
#
# Usage:
#   make          - build the simulator
#   make clean    - remove build artifacts
#   make run      - quick test with synthetic trace

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c99
TARGET  = tempograph
SRCS    = main.c lru.c lfu.c arc.c tempograph.c
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build successful: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Quick test: generate synthetic trace and run all algorithms
run: $(TARGET)
	python3 gen_trace.py 50000 1000 > test_trace.txt
	./$(TARGET) -a all -f 64 test_trace.txt

clean:
	rm -f $(OBJS) $(TARGET) *.csv test_trace.txt
	@echo "Cleaned."

.PHONY: clean run
