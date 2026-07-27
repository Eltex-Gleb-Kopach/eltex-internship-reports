CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

TARGET = priority_queue
TEST_TARGET = tests/test_priority_queue

.PHONY: all clean test

all: $(TARGET)

$(TARGET): main.o priority_queue.o
	$(CC) main.o priority_queue.o -o $(TARGET)

main.o: main.c priority_queue.h
	$(CC) $(CFLAGS) -c main.c

priority_queue.o: priority_queue.c priority_queue.h
	$(CC) $(CFLAGS) -c priority_queue.c

$(TEST_TARGET): tests/test_priority_queue.c priority_queue.c priority_queue.h
	$(CC) $(CFLAGS) tests/test_priority_queue.c priority_queue.c -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f main.o priority_queue.o $(TARGET) $(TEST_TARGET)
