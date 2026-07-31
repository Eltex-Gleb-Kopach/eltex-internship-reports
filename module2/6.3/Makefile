# Компилятор C и общие параметры компиляции.
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

# Имена основной программы, объектных и тестового файлов.
TARGET = calculator
OBJECTS = main.o plugin_loader.o
TEST_TARGET = tests/test_calculator

# Динамические библиотеки с отдельными операциями калькулятора.
PLUGIN_DIR = plugins
PLUGINS = \
	$(PLUGIN_DIR)/libadd.so \
	$(PLUGIN_DIR)/libsubtract.so \
	$(PLUGIN_DIR)/libmultiply.so \
	$(PLUGIN_DIR)/libdivide.so

.PHONY: all test clean

# Цель по умолчанию собирает программу и все плагины.
all: $(TARGET) $(PLUGINS)

# Основная программа использует dlopen, поэтому подключаем libdl.
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -ldl -o $(TARGET)

main.o: main.c calculator.h plugin_loader.h
	$(CC) $(CFLAGS) -c main.c

plugin_loader.o: plugin_loader.c plugin_loader.h calculator.h
	$(CC) $(CFLAGS) -c plugin_loader.c

# Каждый исходник операции собирается в отдельную динамическую библиотеку.
$(PLUGIN_DIR)/lib%.so: plugin_sources/%.c | $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -fPIC -shared $< -o $@

# Создаём каталог автоматически, если он ещё не существует.
$(PLUGIN_DIR):
	mkdir -p $(PLUGIN_DIR)

# Тесты используют тот же загрузчик и готовые библиотеки.
$(TEST_TARGET): tests/test_calculator.c plugin_loader.c \
		plugin_loader.h calculator.h $(PLUGINS)
	$(CC) $(CFLAGS) tests/test_calculator.c plugin_loader.c \
		-ldl -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Удаляем все файлы, созданные во время сборки.
clean:
	rm -f $(OBJECTS) $(TARGET) $(PLUGINS) $(TEST_TARGET)
