# Компилятор C.
CC = gcc

# Предупреждения компилятора и используемый стандарт языка.
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

# Имена исполняемой программы, библиотеки и тестового файла.
TARGET = phonebook
LIBRARY = libcontactlist.so
TEST_TARGET = tests/test_phonebook

# Эти цели являются командами, а не создаваемыми файлами.
.PHONY: all run test clean

# Цель по умолчанию собирает динамическую библиотеку и программу.
all: $(TARGET)

# Линкуем main.o с динамической библиотекой libcontactlist.so.
$(TARGET): main.o $(LIBRARY)
	$(CC) main.o -L. -lcontactlist -o $(TARGET)

# Компилируем файл с функцией main отдельно от библиотеки.
main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c

# Создаём позиционно-независимый объектный файл для библиотеки.
phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -fPIC -c phonebook.c

# Создаём динамическую библиотеку из объектного файла.
$(LIBRARY): phonebook.o
	$(CC) -shared phonebook.o -o $(LIBRARY)

# Запускаем программу, указав каталог с динамической библиотекой.
run: $(TARGET)
	LD_LIBRARY_PATH=. ./$(TARGET)

# Собираем тесты и связываем их с динамической библиотекой.
$(TEST_TARGET): tests/test_phonebook.c phonebook.h $(LIBRARY)
	$(CC) $(CFLAGS) tests/test_phonebook.c -L. -lcontactlist -o $(TEST_TARGET)

# Запускаем тесты, указав каталог с динамической библиотекой.
test: $(TEST_TARGET)
	LD_LIBRARY_PATH=. ./$(TEST_TARGET)

# Удаляем все файлы, созданные во время сборки.
clean:
	rm -f main.o phonebook.o $(LIBRARY) $(TARGET) $(TEST_TARGET)
