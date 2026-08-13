CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
TARGET = p2p_chat
SOURCES = main.c p2p_chat.c
HEADERS = p2p_chat.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) -pthread -lrt

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./tests/test.sh

.PHONY: all clean test
