#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
PROGRAM="$PROJECT_DIR/shared_list"

SHARED_MEMORY_FILE="/dev/shm/eltex_task5_memory"
SEMAPHORE_FILE="/dev/shm/sem.eltex_task5_semaphore"

TEST_DIRECTORY=$(mktemp -d)
BACKGROUND_PIDS=""

cleanup_ipc()
{
    # Удаляем только POSIX-объекты, принадлежащие этому заданию.
    rm -f -- "$SHARED_MEMORY_FILE" "$SEMAPHORE_FILE"
}

cleanup()
{
    for process_id in $BACKGROUND_PIDS; do
        kill "$process_id" >/dev/null 2>&1 || true
        wait "$process_id" >/dev/null 2>&1 || true
    done

    cleanup_ipc
    rm -rf "$TEST_DIRECTORY"
}

trap cleanup EXIT INT TERM

print_test_header()
{
    printf '\n============================================================\n'
    printf '%s\n' "$1"
    printf '============================================================\n'
}

print_log()
{
    sed 's/^/  /' "$1"
}

fail_test()
{
    printf '\n[ОШИБКА] %s\n' "$1" >&2
    exit 1
}

pass_test()
{
    printf '[УСПЕХ] %s\n' "$1"
}

ipc_resources_exist()
{
    [ -e "$SHARED_MEMORY_FILE" ] || [ -e "$SEMAPHORE_FILE" ]
}

wait_for_producer()
{
    for _ in $(seq 1 100); do
        if [ -e "$SHARED_MEMORY_FILE" ] \
            && [ -e "$SEMAPHORE_FILE" ]; then
            return 0
        fi

        sleep 0.05
    done

    return 1
}

start_producer()
{
    local output_file=$1

    timeout 20s "$PROGRAM" producer \
        >"$output_file" 2>&1 &

    STARTED_PRODUCER_PID=$!
    BACKGROUND_PIDS="$BACKGROUND_PIDS $STARTED_PRODUCER_PID"

    wait_for_producer
}

extract_created_count()
{
    sed -n 's/.*Создано блоков: \([0-9][0-9]*\)\..*/\1/p' "$1" \
        | tail -n 1
}

extract_processed_count()
{
    sed -n 's/.*Этот потребитель обработал блоков: \([0-9][0-9]*\)\..*/\1/p' "$1" \
        | tail -n 1
}

# Перед тестами удаляем только старые POSIX-объекты этой лабораторной.
cleanup_ipc

# Тест 1: программа обязана отклонить запуск без указания роли.
print_test_header "Тест 1: запуск без аргументов"

if "$PROGRAM" >"$TEST_DIRECTORY/no_arguments.log" 2>&1; then
    fail_test "Запуск без аргументов завершился успешно."
fi

print_log "$TEST_DIRECTORY/no_arguments.log"

grep -q "Использование:" "$TEST_DIRECTORY/no_arguments.log" \
    || fail_test "Не выведена подсказка по запуску."

pass_test "Запуск без аргументов обработан правильно."

# Тест 2: неизвестный режим должен завершаться ошибкой.
print_test_header "Тест 2: неизвестный режим"

if "$PROGRAM" unknown >"$TEST_DIRECTORY/unknown.log" 2>&1; then
    fail_test "Неизвестный режим был принят программой."
fi

print_log "$TEST_DIRECTORY/unknown.log"

grep -q "неизвестный режим" "$TEST_DIRECTORY/unknown.log" \
    || fail_test "Нет сообщения о неизвестном режиме."

pass_test "Неизвестный режим отклонён."

# Тест 3: потребитель не должен создавать ресурсы самостоятельно.
print_test_header "Тест 3: потребитель без производителя"

if "$PROGRAM" consumer \
    >"$TEST_DIRECTORY/consumer_without_producer.log" 2>&1; then
    fail_test "Потребитель запустился без производителя."
fi

print_log "$TEST_DIRECTORY/consumer_without_producer.log"

grep -q "производитель ещё не запущен" \
    "$TEST_DIRECTORY/consumer_without_producer.log" \
    || fail_test "Нет диагностики отсутствующего производителя."

pass_test "Потребитель правильно обнаружил отсутствие производителя."

# Тест 4: одновременно разрешён только один производитель.
# После проверки запускаем потребителя, чтобы первый производитель
# штатно завершил работу и удалил ресурсы.
print_test_header "Тест 4: защита от второго производителя"

start_producer "$TEST_DIRECTORY/first_producer.log" \
    || fail_test "Первый производитель не создал IPC-ресурсы."

FIRST_PRODUCER_PID=$STARTED_PRODUCER_PID

if "$PROGRAM" producer \
    >"$TEST_DIRECTORY/second_producer.log" 2>&1; then
    fail_test "Удалось запустить второго производителя."
fi

print_log "$TEST_DIRECTORY/second_producer.log"

grep -q "семафор уже существует" \
    "$TEST_DIRECTORY/second_producer.log" \
    || fail_test "Нет диагностики второго производителя."

timeout 20s "$PROGRAM" consumer \
    >"$TEST_DIRECTORY/single_consumer.log" 2>&1

SINGLE_CONSUMER_STATUS=$?

wait "$FIRST_PRODUCER_PID"
FIRST_PRODUCER_STATUS=$?

if [ "$SINGLE_CONSUMER_STATUS" -ne 0 ] \
    || [ "$FIRST_PRODUCER_STATUS" -ne 0 ]; then
    print_log "$TEST_DIRECTORY/first_producer.log"
    print_log "$TEST_DIRECTORY/single_consumer.log"
    fail_test "Штатное завершение после проверки второго производителя не удалось."
fi

CREATED_COUNT=$(extract_created_count \
    "$TEST_DIRECTORY/first_producer.log")
SINGLE_PROCESSED_COUNT=$(extract_processed_count \
    "$TEST_DIRECTORY/single_consumer.log")

if [ -z "$CREATED_COUNT" ] \
    || [ -z "$SINGLE_PROCESSED_COUNT" ] \
    || [ "$CREATED_COUNT" -ne "$SINGLE_PROCESSED_COUNT" ]; then
    fail_test "Один потребитель обработал не все созданные блоки."
fi

printf '  Создано блоков: %s\n' "$CREATED_COUNT"
printf '  Обработано одним потребителем: %s\n' \
    "$SINGLE_PROCESSED_COUNT"

ipc_resources_exist \
    && fail_test "После завершения остались IPC-ресурсы."

pass_test "Второй производитель отклонён, один потребитель обработал все блоки."

# Тест 5: два потребителя должны совместно обработать все блоки,
# причём сумма их результатов должна совпасть с числом созданных блоков.
print_test_header "Тест 5: два потребителя"

start_producer "$TEST_DIRECTORY/multiple_producer.log" \
    || fail_test "Производитель не создал IPC-ресурсы."

MULTIPLE_PRODUCER_PID=$STARTED_PRODUCER_PID

timeout 20s "$PROGRAM" consumer \
    >"$TEST_DIRECTORY/consumer_1.log" 2>&1 &
CONSUMER_1_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $CONSUMER_1_PID"

timeout 20s "$PROGRAM" consumer \
    >"$TEST_DIRECTORY/consumer_2.log" 2>&1 &
CONSUMER_2_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $CONSUMER_2_PID"

wait "$CONSUMER_1_PID"
CONSUMER_1_STATUS=$?

wait "$CONSUMER_2_PID"
CONSUMER_2_STATUS=$?

wait "$MULTIPLE_PRODUCER_PID"
MULTIPLE_PRODUCER_STATUS=$?

if [ "$CONSUMER_1_STATUS" -ne 0 ] \
    || [ "$CONSUMER_2_STATUS" -ne 0 ] \
    || [ "$MULTIPLE_PRODUCER_STATUS" -ne 0 ]; then
    fail_test "Один из процессов завершился с ошибкой."
fi

MULTIPLE_CREATED_COUNT=$(extract_created_count \
    "$TEST_DIRECTORY/multiple_producer.log")
CONSUMER_1_COUNT=$(extract_processed_count \
    "$TEST_DIRECTORY/consumer_1.log")
CONSUMER_2_COUNT=$(extract_processed_count \
    "$TEST_DIRECTORY/consumer_2.log")

if [ -z "$MULTIPLE_CREATED_COUNT" ] \
    || [ -z "$CONSUMER_1_COUNT" ] \
    || [ -z "$CONSUMER_2_COUNT" ]; then
    fail_test "Не удалось прочитать счётчики процессов."
fi

TOTAL_PROCESSED_COUNT=$((CONSUMER_1_COUNT + CONSUMER_2_COUNT))

printf '  Создано блоков: %s\n' "$MULTIPLE_CREATED_COUNT"
printf '  Потребитель 1: %s\n' "$CONSUMER_1_COUNT"
printf '  Потребитель 2: %s\n' "$CONSUMER_2_COUNT"
printf '  Всего обработано: %s\n' "$TOTAL_PROCESSED_COUNT"

if [ "$TOTAL_PROCESSED_COUNT" -ne "$MULTIPLE_CREATED_COUNT" ]; then
    fail_test "Сумма обработанных блоков не совпадает с количеством созданных."
fi

ipc_resources_exist \
    && fail_test "После завершения остались IPC-ресурсы."

pass_test "Два потребителя совместно обработали все блоки."

printf '\nВсе тесты пройдены: 5.\n'
