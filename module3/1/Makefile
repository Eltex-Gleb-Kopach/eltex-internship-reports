CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
TARGET = file_copy
SOURCES = main.c file_copy.c
HEADERS = file_copy.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

test: $(TARGET)
	./tests/test.sh

clean:
	rm -f $(TARGET) *.copy

.PHONY: all test clean
