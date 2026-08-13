#!/usr/bin/env bash

set -u

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
PROGRAM="./p2p_chat"
TEST_DIR=$(mktemp -d /tmp/p2p-chat-tests.XXXXXX)

PASSED_TESTS=0
FIRST_PID=""
SECOND_PID=""

# Для каждого теста используется своё имя, чтобы очереди не пересекались.
NORMAL_NAME="p2p_normal_$$"
LONG_NAME="p2p_long_$$"
SIGNAL_NAME="p2p_signal_$$"
SINGLE_NAME="p2p_single_$$"
LONE_EXIT_NAME="p2p_lone_exit_$$"

cleanup()
{
    for process_pid in "$FIRST_PID" "$SECOND_PID"; do
        if [[ -n "$process_pid" ]] \
            && kill -0 "$process_pid" 2>/dev/null; then
            kill -KILL "$process_pid" 2>/dev/null || true
            wait "$process_pid" 2>/dev/null || true
        fi
    done

    exec 8>&- 2>/dev/null || true
    exec 9>&- 2>/dev/null || true

    # Удаляем только очереди с уникальными именами этого запуска тестов.
    for queue_name in \
        "${NORMAL_NAME}_1" "${NORMAL_NAME}_2" \
        "${LONG_NAME}_1" "${LONG_NAME}_2" \
        "${SIGNAL_NAME}_1" "${SIGNAL_NAME}_2" \
        "${SINGLE_NAME}_1" "${SINGLE_NAME}_2" \
        "${LONE_EXIT_NAME}_1" "${LONE_EXIT_NAME}_2"; do
        rm -f -- "/dev/mqueue/$queue_name" 2>/dev/null || true
    done

    if [[ "$TEST_DIR" == /tmp/p2p-chat-tests.* ]] \
        && [[ -d "$TEST_DIR" ]]; then
        rm -rf -- "$TEST_DIR"
    fi
}

trap cleanup EXIT

fail()
{
    printf '\n[ОШИБКА] %s\n' "$1" >&2
    exit 1
}

pass()
{
    PASSED_TESTS=$((PASSED_TESTS + 1))
    printf '\n[УСПЕХ] Тест %d пройден: %s\n' \
        "$PASSED_TESTS" "$1"
}

# Отделяет тесты, чтобы вывод не сливался в одну простыню.
start_test()
{
    local test_number=$1
    local test_name=$2

    printf '\n============================================================\n'
    printf 'ТЕСТ %s: %s\n' "$test_number" "$test_name"
    printf '============================================================\n'
}

# Показывает вывод конкретного процесса с небольшим отступом.
show_output()
{
    local title=$1
    local log_file=$2

    printf '%s\n' "$title"

    if [[ -s "$log_file" ]]; then
        sed 's/^/  /' "$log_file"
    else
        printf '  (программа ничего не вывела)\n'
    fi
}

# Не позволяет тесту навсегда зависнуть при ошибке в программе.
wait_for_process()
{
    local process_pid=$1
    local description=$2

    if ! timeout 8s tail --pid="$process_pid" -f /dev/null; then
        fail "$description не завершился за отведённое время"
    fi

    if ! wait "$process_pid"; then
        fail "$description завершился с ошибкой"
    fi
}

queues_removed()
{
    local base_name=$1

    [[ ! -e "/dev/mqueue/${base_name}_1" \
        && ! -e "/dev/mqueue/${base_name}_2" ]]
}

# Запускаем программу из папки задания, поэтому в подсказке
# отображается короткий путь ./p2p_chat.
cd -- "$PROJECT_DIR" \
    || fail "не удалось открыть папку задания"

[[ -x "$PROGRAM" ]] \
    || fail "исполняемый файл p2p_chat не найден; сначала выполни make"

# Тест 1: программа должна требовать имя очереди.
start_test 1 "Запуск без имени очереди"

if "$PROGRAM" >"$TEST_DIR/no_args.log" 2>&1; then
    fail "программа запустилась без имени очереди"
fi

grep -Fq "Использование:" "$TEST_DIR/no_args.log" \
    || fail "не показана подсказка по запуску"

show_output "Вывод программы:" "$TEST_DIR/no_args.log"
pass "запуск без аргумента отклонён"

# Тест 2: имя POSIX-очереди не должно содержать внутренний '/'.
start_test 2 "Неправильное имя очереди"

if "$PROGRAM" bad/name >"$TEST_DIR/bad_name.log" 2>&1; then
    fail "программа приняла неправильное имя очереди"
fi

grep -Fq "неправильное имя" "$TEST_DIR/bad_name.log" \
    || fail "не показано сообщение о неправильном имени"

show_output "Вывод программы:" "$TEST_DIR/bad_name.log"
pass "неправильное имя очереди отклонено"

# Тест 3: оба участника передают по два обычных сообщения и /exit.
start_test 3 "Несколько сообщений в обе стороны"

{
    printf 'A1\nA2\n'
    # Держим stdin открытым, пока второй участник не отправит /exit.
    sleep 1
} | "$PROGRAM" "$NORMAL_NAME" \
    >"$TEST_DIR/normal_first.log" 2>&1 &
FIRST_PID=$!

sleep 0.2

{
    printf 'B1\nB2\n'
    sleep 0.3
    printf '/exit\n'
} | "$PROGRAM" "$NORMAL_NAME" \
    >"$TEST_DIR/normal_second.log" 2>&1 &
SECOND_PID=$!

wait_for_process "$FIRST_PID" "первый участник"
FIRST_PID=""
wait_for_process "$SECOND_PID" "второй участник"
SECOND_PID=""

grep -Fq "Получено сообщение: B1" "$TEST_DIR/normal_first.log" \
    || fail "первый участник не получил B1"
grep -Fq "Получено сообщение: B2" "$TEST_DIR/normal_first.log" \
    || fail "первый участник не получил B2"
grep -Fq "Получено сообщение: A1" "$TEST_DIR/normal_second.log" \
    || fail "второй участник не получил A1"
grep -Fq "Получено сообщение: A2" "$TEST_DIR/normal_second.log" \
    || fail "второй участник не получил A2"

queues_removed "$NORMAL_NAME" \
    || fail "создатель не удалил очереди после /exit"

show_output "Первый участник:" "$TEST_DIR/normal_first.log"
show_output "Второй участник:" "$TEST_DIR/normal_second.log"
pass "двусторонний обмен и завершение через /exit"

# Тест 4: длинная строка отклоняется, а следующая передаётся нормально.
start_test 4 "Слишком длинное сообщение"

{
    printf '%1100s\n' '' | tr ' ' x
    printf 'Корректное сообщение\n/exit\n'
} | "$PROGRAM" "$LONG_NAME" \
        >"$TEST_DIR/long_first.log" 2>&1 &
FIRST_PID=$!

sleep 0.2

printf 'Ответ\n/exit\n' \
    | "$PROGRAM" "$LONG_NAME" \
        >"$TEST_DIR/long_second.log" 2>&1 &
SECOND_PID=$!

wait_for_process "$FIRST_PID" "первый участник"
FIRST_PID=""
wait_for_process "$SECOND_PID" "второй участник"
SECOND_PID=""

grep -Fq "сообщение слишком длинное" "$TEST_DIR/long_first.log" \
    || fail "длинное сообщение не было отклонено"
grep -Fq "Получено сообщение: Корректное сообщение" \
    "$TEST_DIR/long_second.log" \
    || fail "после ошибки не передалось корректное сообщение"

queues_removed "$LONG_NAME" \
    || fail "очереди не удалены после проверки длинного сообщения"

show_output "Первый участник:" "$TEST_DIR/long_first.log"
show_output "Второй участник:" "$TEST_DIR/long_second.log"
pass "длинная строка отклонена без нарушения следующей передачи"

# Тест 5: SIGINT одному участнику должен завершить обе стороны.
start_test 5 "Ctrl+C при двух участниках"

mkfifo "$TEST_DIR/signal_first.in" "$TEST_DIR/signal_second.in"

"$PROGRAM" "$SIGNAL_NAME" \
    <"$TEST_DIR/signal_first.in" \
    >"$TEST_DIR/signal_first.log" 2>&1 &
FIRST_PID=$!
exec 8>"$TEST_DIR/signal_first.in"

"$PROGRAM" "$SIGNAL_NAME" \
    <"$TEST_DIR/signal_second.in" \
    >"$TEST_DIR/signal_second.log" 2>&1 &
SECOND_PID=$!
exec 9>"$TEST_DIR/signal_second.in"

sleep 0.3
kill -INT "$FIRST_PID"

wait_for_process "$FIRST_PID" "участник, получивший SIGINT"
FIRST_PID=""
wait_for_process "$SECOND_PID" "вторая сторона"
SECOND_PID=""
exec 8>&-
exec 9>&-

grep -Fq "Уведомление о завершении отправлено" \
    "$TEST_DIR/signal_first.log" \
    || fail "после SIGINT не отправлено уведомление"
grep -Fq "Вторая сторона завершает работу" \
    "$TEST_DIR/signal_second.log" \
    || fail "вторая сторона не получила уведомление"

queues_removed "$SIGNAL_NAME" \
    || fail "очереди не удалены после SIGINT"

show_output "Участник, получивший SIGINT:" \
    "$TEST_DIR/signal_first.log"
show_output "Вторая сторона:" "$TEST_DIR/signal_second.log"
pass "SIGINT корректно завершил обе стороны"

# Тест 6: создатель не должен зависать по SIGINT без второй стороны.
start_test 6 "Ctrl+C без второго участника"

mkfifo "$TEST_DIR/single.in"

"$PROGRAM" "$SINGLE_NAME" \
    <"$TEST_DIR/single.in" \
    >"$TEST_DIR/single.log" 2>&1 &
FIRST_PID=$!
exec 8>"$TEST_DIR/single.in"

sleep 0.3
kill -INT "$FIRST_PID"

wait_for_process "$FIRST_PID" "одинокий участник"
FIRST_PID=""
exec 8>&-

queues_removed "$SINGLE_NAME" \
    || fail "одинокий создатель не удалил очереди"

show_output "Вывод программы:" "$TEST_DIR/single.log"
pass "одинокий участник корректно завершился по SIGINT"

# Тест 7: команда /exit не должна требовать обязательного ответа второй стороны.
start_test 7 "/exit без второго участника"

printf '/exit\n' \
    | "$PROGRAM" "$LONE_EXIT_NAME" \
        >"$TEST_DIR/lone_exit.log" 2>&1 &
FIRST_PID=$!

wait_for_process "$FIRST_PID" "одинокий участник с командой /exit"
FIRST_PID=""

grep -Fq "Уведомление о завершении отправлено" \
    "$TEST_DIR/lone_exit.log" \
    || fail "команда /exit не отправила уведомление"

queues_removed "$LONE_EXIT_NAME" \
    || fail "очереди не удалены после одиночной команды /exit"

show_output "Вывод программы:" "$TEST_DIR/lone_exit.log"
pass "одинокий участник корректно завершился через /exit"

printf '\nВсе тесты пройдены: %d.\n' "$PASSED_TESTS"
