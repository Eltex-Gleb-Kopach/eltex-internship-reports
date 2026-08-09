#!/usr/bin/env bash


set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
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

# Показывает сохранённый вывод проверяемой программы.
show_program_output()
{
    local stdout_file=$1
    local stderr_file=$2

    if [[ -s "$stdout_file" ]]; then
        echo "Вывод stdout:"
        sed 's/^/  /' "$stdout_file"
    fi

    if [[ -s "$stderr_file" ]]; then
        echo "Вывод stderr:"
        sed 's/^/  /' "$stderr_file"
    fi
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

# Тест 1: сборка программы.
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

# Создаём четыре разных исходных файла.
printf 'Первая строка\nВторая строка\n' > text.txt
: > empty.txt
printf '\x00\x01\x02\x7f\x80\xff' > binary.bin
dd if=/dev/urandom of=large.bin bs=10000 count=1 status=none

# Тест 2: запуск без аргументов.
echo
echo "Проверка запуска без аргументов..."

if "$PROGRAM" >no_args.stdout 2>no_args.stderr; then
    fail "Программа успешно завершилась без списка файлов."
fi

show_program_output no_args.stdout no_args.stderr

if ! grep -q "не указаны файлы" no_args.stderr; then
    fail "Диагностика запуска без файлов не записана в stderr."
fi

pass "Запуск без файлов корректно завершился с ошибкой."

# Тест 3: запуск с неизвестным ключом.
echo
echo "Проверка неизвестного ключа..."

if "$PROGRAM" -x text.txt >unknown_key.stdout 2>unknown_key.stderr; then
    fail "Программа приняла неизвестный ключ -x."
fi

show_program_output unknown_key.stdout unknown_key.stderr

if ! grep -q "неизвестный ключ" unknown_key.stderr; then
    fail "Диагностика неизвестного ключа не записана в stderr."
fi

pass "Неизвестный ключ корректно обработан."

# Тест 4: запуск с неполными аргументами ключа -p.
echo
echo "Проверка неполных аргументов ключа -p..."

if "$PROGRAM" -p >missing_fifo_args.stdout 2>missing_fifo_args.stderr; then
    fail "Программа приняла -p без имени канала и файла."
fi

echo "Случай: ключ -p без имени канала и файла"
show_program_output missing_fifo_args.stdout missing_fifo_args.stderr

if ! grep -q "после -p" missing_fifo_args.stderr; then
    fail "Диагностика неполных аргументов -p не записана в stderr."
fi

if "$PROGRAM" -p test_channel >missing_file_arg.stdout 2>missing_file_arg.stderr; then
    fail "Программа приняла -p с именем канала, но без файла."
fi

echo "Случай: после -p указано только имя канала"
show_program_output missing_file_arg.stdout missing_file_arg.stderr

if ! grep -q "после -p" missing_file_arg.stderr; then
    fail "Диагностика отсутствующего файла после -p не записана в stderr."
fi

pass "Неполные аргументы ключа -p корректно обработаны."

# Тест 5: копирование через неименованный pipe.
echo
echo "Проверка режима с неименованным каналом..."

if ! "$PROGRAM" text.txt empty.txt binary.bin large.bin; then
    fail "Программа завершилась с ошибкой в обычном режиме."
fi

check_copy text.txt text.txt.copy
check_copy empty.txt empty.txt.copy
check_copy binary.bin binary.bin.copy
check_copy large.bin large.bin.copy

pass "Неименованный канал правильно скопировал четыре файла."

# Удаляем только копии предыдущей проверки,
# чтобы следующий запуск создал их заново.
rm -- text.txt.copy empty.txt.copy binary.bin.copy large.bin.copy

# Тест 6: копирование через именованный FIFO.
echo
echo "Проверка режима с именованным каналом FIFO..."

if ! "$PROGRAM" -p test_channel text.txt empty.txt binary.bin large.bin; then
    fail "Программа завершилась с ошибкой в режиме FIFO."
fi

check_copy text.txt text.txt.copy
check_copy empty.txt empty.txt.copy
check_copy binary.bin binary.bin.copy
check_copy large.bin large.bin.copy

# После нормального завершения файл FIFO должен быть удалён.
if [[ -e test_channel ]]; then
    fail "FIFO test_channel не был удалён после завершения."
fi

pass "FIFO правильно скопировал файлы."

# Тест 7: отсутствующий файл при использовании неименованного pipe.
echo
echo "Проверка отсутствующего файла с неименованным каналом..."

# Программа должна вернуть код ошибки и корректно завершить оба процесса,
# даже если для передачи используется обычный неименованный pipe.
if "$PROGRAM" missing.txt >missing_pipe.stdout 2>missing_pipe.stderr; then
    fail "Неименованный канал не сообщил об отсутствующем файле."
fi

show_program_output missing_pipe.stdout missing_pipe.stderr

if ! grep -q "missing.txt" missing_pipe.stderr; then
    fail "Сообщение об отсутствующем файле не записано в stderr."
fi

pass "Неименованный канал корректно обработал отсутствующий файл."

# Тест 8: отсутствующий файл при использовании FIFO.
echo
echo "Проверка отсутствующего файла с именованным каналом FIFO..."

# Здесь программа обязана вернуть код ошибки, потому что missing.txt
# не существует. Поэтому успешное завершение считаем ошибкой теста.
if "$PROGRAM" -p error_channel text.txt missing.txt \
    >missing_fifo.stdout 2>missing_fifo.stderr; then
    fail "FIFO не сообщил об отсутствующем файле."
fi

show_program_output missing_fifo.stdout missing_fifo.stderr

if ! grep -q "missing.txt" missing_fifo.stderr; then
    fail "Сообщение об отсутствующем файле в режиме FIFO не записано в stderr."
fi

# FIFO должен удаляться не только после успеха, но и после ошибки.
if [[ -e error_channel ]]; then
    fail "FIFO error_channel не был удалён после ошибки."
fi

pass "FIFO обработал отсутствующий файл и удалил временный канал."

# Тест 9: имя FIFO уже занято обычным файлом.
echo
echo "Проверка занятого имени FIFO..."

printf 'do not delete\n' > occupied_channel

if "$PROGRAM" -p occupied_channel text.txt \
    >occupied_fifo.stdout 2>occupied_fifo.stderr; then
    fail "Программа создала FIFO поверх существующего файла."
fi

show_program_output occupied_fifo.stdout occupied_fifo.stderr

if [[ ! -f occupied_channel ]] \
    || ! grep -qx "do not delete" occupied_channel; then
    fail "Программа удалила или изменила файл с занятым именем FIFO."
fi

if [[ ! -s occupied_fifo.stderr ]]; then
    fail "Ошибка создания FIFO не записана в stderr."
fi

rm -- occupied_channel

pass "Занятое имя FIFO обработано без удаления чужого файла."

echo
echo "Все тесты пройдены: $passed_tests."
