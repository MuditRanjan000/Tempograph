# Makefile - TempoGraph Simulation Framework
# CSD 204 - Operating Systems Project
# Group 12: Mudit Ranjan, Mugdh Mittal, Aayush Trivedi

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c99
TARGET  = tempograph
SRCS    = main.c lru.c lfu.c arc.c tempograph.c sieve.c
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build successful: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	python3 gen_trace.py 100000 255 --type scan --out traces/scan.txt
	./$(TARGET) -a all -f 64 traces/scan.txt

clean:
	rm -f $(OBJS) $(TARGET) *.csv
	@echo "Cleaned."

.PHONY: clean run