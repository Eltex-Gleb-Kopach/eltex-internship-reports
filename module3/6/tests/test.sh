#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
PROGRAM="$PROJECT_DIR/udp_chat"

TEST_DIRECTORY=$(mktemp -d)
BACKGROUND_PIDS=""

cleanup()
{
    for process_id in $BACKGROUND_PIDS; do
        kill "$process_id" >/dev/null 2>&1 || true
        wait "$process_id" >/dev/null 2>&1 || true
    done

    rm -rf -- "$TEST_DIRECTORY"
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

# Тест 1: программа должна требовать имя участника.
print_test_header "Тест 1: запуск без имени"

if "$PROGRAM" >"$TEST_DIRECTORY/no_name.log" 2>&1; then
    fail_test "Программа разрешила запуск без имени."
fi

print_log "$TEST_DIRECTORY/no_name.log"

grep -q "Использование:" "$TEST_DIRECTORY/no_name.log" \
    || fail_test "Не выведена подсказка по запуску."

pass_test "Запуск без имени обработан правильно."

# Тест 2: символ-разделитель протокола нельзя использовать в имени.
print_test_header "Тест 2: имя с символом-разделителем"

if "$PROGRAM" 'Bad|Name' \
    >"$TEST_DIRECTORY/bad_separator.log" 2>&1; then
    fail_test "Программа приняла имя с символом '|'."
fi

print_log "$TEST_DIRECTORY/bad_separator.log"

grep -q "неправильное имя" \
    "$TEST_DIRECTORY/bad_separator.log" \
    || fail_test "Нет диагностики неправильного имени."

pass_test "Имя с разделителем отклонено."

# Тест 3: имя не должно помещаться за границами USER_NAME_SIZE.
print_test_header "Тест 3: слишком длинное имя"

LONG_NAME="abcdefghijklmnopqrstuvwxyz123456"

if "$PROGRAM" "$LONG_NAME" \
    >"$TEST_DIRECTORY/long_name.log" 2>&1; then
    fail_test "Программа приняла слишком длинное имя."
fi

print_log "$TEST_DIRECTORY/long_name.log"

grep -q "неправильное имя" \
    "$TEST_DIRECTORY/long_name.log" \
    || fail_test "Нет диагностики слишком длинного имени."

pass_test "Слишком длинное имя отклонено."

# Тест 4: два клиента должны увидеть JOIN, MESSAGE и LEAVE.
print_test_header "Тест 4: обмен между двумя UDP-клиентами"

ALICE_INPUT="$TEST_DIRECTORY/alice.input"
BOB_INPUT="$TEST_DIRECTORY/bob.input"

mkfifo "$ALICE_INPUT" "$BOB_INPUT"

# Открытие FIFO на чтение и запись не даёт клиентам получить EOF раньше времени.
exec 3<>"$ALICE_INPUT"
exec 4<>"$BOB_INPUT"

timeout 8s "$PROGRAM" Alice \
    <"$ALICE_INPUT" \
    >"$TEST_DIRECTORY/alice.log" 2>&1 &
ALICE_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $ALICE_PID"

sleep 0.3

timeout 8s "$PROGRAM" Bob \
    <"$BOB_INPUT" \
    >"$TEST_DIRECTORY/bob.log" 2>&1 &
BOB_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $BOB_PID"

sleep 0.3

printf 'Привет от Alice\n' >&3
sleep 0.3

printf '/exit\n' >&4
wait "$BOB_PID"
BOB_STATUS=$?

sleep 0.3

printf '/exit\n' >&3
wait "$ALICE_PID"
ALICE_STATUS=$?

exec 3>&-
exec 4>&-

printf '%s\n' "--- Вывод Alice ---"
print_log "$TEST_DIRECTORY/alice.log"
printf '%s\n' "--- Вывод Bob ---"
print_log "$TEST_DIRECTORY/bob.log"

if [ "$ALICE_STATUS" -ne 0 ] || [ "$BOB_STATUS" -ne 0 ]; then
    fail_test "Один из клиентов завершился с ошибкой."
fi

grep -q "В сети появился участник Bob" \
    "$TEST_DIRECTORY/alice.log" \
    || fail_test "Alice не получила сообщение JOIN от Bob."

grep -q "\[Alice\] Привет от Alice" \
    "$TEST_DIRECTORY/bob.log" \
    || fail_test "Bob не получил обычное сообщение от Alice."

grep -q "Участник Bob вышел из сети" \
    "$TEST_DIRECTORY/alice.log" \
    || fail_test "Alice не получила сообщение LEAVE от Bob."

pass_test "JOIN, MESSAGE и LEAVE переданы между клиентами."

# Тест 5: SIGINT должен привести к отправке LEAVE и штатному завершению.
print_test_header "Тест 5: завершение по SIGINT"

SIGNAL_INPUT="$TEST_DIRECTORY/signal.input"
mkfifo "$SIGNAL_INPUT"
exec 5<>"$SIGNAL_INPUT"

"$PROGRAM" SignalClient \
    <"$SIGNAL_INPUT" \
    >"$TEST_DIRECTORY/signal.log" 2>&1 &
SIGNAL_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $SIGNAL_PID"

# Страховочный процесс завершит зависший тест через пять секунд.
(
    sleep 5
    kill -TERM "$SIGNAL_PID" >/dev/null 2>&1 || true
) &
WATCHDOG_PID=$!
BACKGROUND_PIDS="$BACKGROUND_PIDS $WATCHDOG_PID"

sleep 0.3
kill -INT "$SIGNAL_PID"

wait "$SIGNAL_PID"
SIGNAL_STATUS=$?

kill "$WATCHDOG_PID" >/dev/null 2>&1 || true
wait "$WATCHDOG_PID" >/dev/null 2>&1 || true
exec 5>&-

print_log "$TEST_DIRECTORY/signal.log"

if [ "$SIGNAL_STATUS" -ne 0 ]; then
    fail_test "Клиент завершился по SIGINT с ошибкой."
fi

grep -q "Получен SIGINT" "$TEST_DIRECTORY/signal.log" \
    || fail_test "Клиент не обработал SIGINT."

grep -q "LEAVE|SignalClient" "$TEST_DIRECTORY/signal.log" \
    || fail_test "При SIGINT не было отправлено сообщение LEAVE."

pass_test "SIGINT приводит к корректному завершению."

printf '\nВсе тесты пройдены: 5.\n'
