CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
TEST_TARGET = tests/test_phonebook

.PHONY: clean test

phonebook: main.o phonebook.o
	$(CC) main.o phonebook.o -o phonebook

main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c

phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -c phonebook.c

$(TEST_TARGET): tests/test_phonebook.c phonebook.c phonebook.h
	$(CC) $(CFLAGS) tests/test_phonebook.c phonebook.c -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f main.o phonebook.o phonebook $(TEST_TARGET)
