# Компилятор C и архиватор для создания статической библиотеки.
CC = gcc
AR = ar

# Предупреждения компилятора, стандарт C и опции архиватора.
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
ARFLAGS = rcs

# Имена исполняемой программы, библиотеки и тестового файла.
TARGET = phonebook
LIBRARY = libphonebook.a
TEST_TARGET = tests/test_phonebook

# Эти цели являются командами, а не создаваемыми файлами.
.PHONY: all clean test

# Цель по умолчанию собирает основную программу.
all: $(TARGET)

# Линкуем main.o со статической библиотекой libphonebook.a.
$(TARGET): main.o $(LIBRARY)
	$(CC) main.o -L. -lphonebook -o $(TARGET)

# Компилируем файл с функцией main отдельно от библиотеки.
main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c

# Компилируем функции телефонной книги в объектный файл.
phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -c phonebook.c

# Упаковываем phonebook.o в статическую библиотеку.
$(LIBRARY): phonebook.o
	$(AR) $(ARFLAGS) $(LIBRARY) phonebook.o

# Собираем тесты и также линкуем их со статической библиотекой.
$(TEST_TARGET): tests/test_phonebook.c phonebook.h $(LIBRARY)
	$(CC) $(CFLAGS) tests/test_phonebook.c -L. -lphonebook -o $(TEST_TARGET)

# Собираем и запускаем все юнит-тесты.
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Удаляем все файлы, созданные во время сборки.
clean:
	rm -f main.o phonebook.o $(LIBRARY) $(TARGET) $(TEST_TARGET)
