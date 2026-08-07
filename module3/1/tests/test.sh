#!/usr/bin/env bash

# Скрипт останавливается при обращении к необъявленной переменной.
set -u

# BASH_SOURCE[0] содержит путь к самому test.sh.
# Благодаря этому скрипт можно запускать из любой текущей папки.
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# Исходные файлы программы находятся на один уровень выше tests.
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

# Все тестовые файлы и собранная программа создаются в /tmp,
# поэтому папка учебного задания не засоряется файлами .copy и FIFO.
TEST_DIR=$(mktemp -d /tmp/file-copy-tests.XXXXXX)

if [[ ! -d "$TEST_DIR" ]]; then
    echo "Ошибка: не удалось создать временную папку." >&2
    exit 1
fi

# При любом завершении скрипта удаляем только созданную нами
# временную папку с известным путём.
cleanup()
{
    rm -rf -- "$TEST_DIR"
}

trap cleanup EXIT

# Исполняемый файл также помещается во временную папку.
PROGRAM="$TEST_DIR/file_copy"

# Счётчик успешно выполненных проверок.
passed_tests=0

# Завершает тестирование при первой обнаруженной ошибке.
fail()
{
    echo "[ОШИБКА] $1" >&2
    exit 1
}

# Выводит название успешно пройденной проверки
# и увеличивает общий счётчик.
pass()
{
    echo "[УСПЕХ] $1"
    ((passed_tests += 1))
}

# Сравнивает оригинал и копию побайтно.
# cmp -s ничего не печатает и возвращает 0 при полном совпадении.
check_copy()
{
    local original=$1
    local copy=$2

    if ! cmp -s -- "$original" "$copy"; then
        fail "Файлы $original и $copy различаются."
    fi
}

echo "Сборка программы..."

# Собираем программу с предупреждениями компилятора.
# Если gcc обнаружит ошибку, тестирование сразу прекращается.
if ! gcc -Wall -Wextra -Wpedantic -std=c11 \
    "$PROJECT_DIR/main.c" \
    "$PROJECT_DIR/file_copy.c" \
    -o "$PROGRAM"; then
    fail "Программа не собрана."
fi

pass "Программа успешно собрана."

# Переходим во временную папку, чтобы все создаваемые копии
# и именованные каналы находились только внутри неё.
cd -- "$TEST_DIR" || fail "Не удалось открыть временную папку."

# Создаём три разных исходных файла:
# обычный текстовый, пустой и содержащий несколько двоичных байтов.
printf 'Первая строка\nВторая строка\n' > text.txt
: > empty.txt
printf '\x00\x01\x02\x7f\x80\xff' > binary.bin

echo
echo "Проверка режима с неименованным каналом..."

if ! "$PROGRAM" text.txt empty.txt binary.bin; then
    fail "Программа завершилась с ошибкой в обычном режиме."
fi

check_copy text.txt text.txt.copy
check_copy empty.txt empty.txt.copy
check_copy binary.bin binary.bin.copy

pass "Неименованный канал правильно скопировал три файла."

# Удаляем только копии предыдущей проверки,
# чтобы следующий запуск создал их заново.
rm -- text.txt.copy empty.txt.copy binary.bin.copy

echo
echo "Проверка режима с именованным каналом FIFO..."

if ! "$PROGRAM" -p test_channel text.txt empty.txt binary.bin; then
    fail "Программа завершилась с ошибкой в режиме FIFO."
fi

check_copy text.txt text.txt.copy
check_copy empty.txt empty.txt.copy
check_copy binary.bin binary.bin.copy

# После нормального завершения файл FIFO должен быть удалён.
if [[ -e test_channel ]]; then
    fail "FIFO test_channel не был удалён после завершения."
fi

pass "FIFO правильно скопировал файлы и был удалён."

echo
echo "Проверка обработки отсутствующего файла..."

# Здесь программа обязана вернуть код ошибки, потому что missing.txt
# не существует. Поэтому успешное завершение считаем ошибкой теста.
if "$PROGRAM" -p error_channel text.txt missing.txt >/dev/null 2>&1; then
    fail "Программа не сообщила об отсутствующем файле."
fi

# FIFO должен удаляться не только после успеха, но и после ошибки.
if [[ -e error_channel ]]; then
    fail "FIFO error_channel не был удалён после ошибки."
fi

pass "Ошибка файла обработана, временный FIFO удалён."

echo
echo "Все тесты пройдены: $passed_tests."
