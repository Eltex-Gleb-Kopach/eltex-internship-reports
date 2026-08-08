CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
TARGET = pubsub
SOURCES = main.c broker.c publisher.c subscriber.c message_queue.c subscriptions.c publisher_list.c signal_control.c
HEADERS = broker.h publisher.h subscriber.h message_queue.h protocol.h subscriptions.h publisher_list.h signal_control.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

test: $(TARGET)
	./tests/test.sh

clean:
	rm -f $(TARGET)

.PHONY: all test clean
