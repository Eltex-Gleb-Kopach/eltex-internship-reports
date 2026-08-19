CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

TARGET = shared_list
OBJECTS = main.o producer.o consumer.o semaphore_control.o

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

main.o: main.c producer.h consumer.h
	$(CC) $(CFLAGS) -c main.c -o main.o

producer.o: producer.c producer.h shared_data.h semaphore_control.h
	$(CC) $(CFLAGS) -c producer.c -o producer.o

consumer.o: consumer.c consumer.h shared_data.h semaphore_control.h
	$(CC) $(CFLAGS) -c consumer.c -o consumer.o

semaphore_control.o: semaphore_control.c semaphore_control.h shared_data.h
	$(CC) $(CFLAGS) -c semaphore_control.c -o semaphore_control.o

test: $(TARGET)
	./tests/test.sh

clean:
	rm -f $(TARGET) $(OBJECTS)
