CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

TARGET = udp_chat
OBJECTS = main.o udp_chat.o

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

main.o: main.c udp_chat.h
	$(CC) $(CFLAGS) -c main.c -o main.o

udp_chat.o: udp_chat.c udp_chat.h
	$(CC) $(CFLAGS) -c udp_chat.c -o udp_chat.o

test: $(TARGET)
	./tests/test.sh

clean:
	rm -f $(TARGET) $(OBJECTS)
