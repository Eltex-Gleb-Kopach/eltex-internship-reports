#!/usr/bin/env bash

set -u

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
PROGRAM="./pubsub"
QUEUE_KEY="0x454c5458"
TEST_TMP_DIR=$(mktemp -d /tmp/pubsub-tests-XXXXXX)

BROKER_PID=""
SUBSCRIBER_PID=""
SECOND_SUBSCRIBER_PID=""
PUBLISHER_PID=""
SECOND_PUBLISHER_PID=""
PASSED_TESTS=0

cleanup()
{
    for process_pid in \
        "$PUBLISHER_PID" \
        "$SECOND_PUBLISHER_PID" \
        "$SUBSCRIBER_PID" \
        "$SECOND_SUBSCRIBER_PID" \
        "$BROKER_PID"; do
        if [[ -n "$process_pid" ]] \
            && kill -0 "$process_pid" 2>/dev/null; then
            kill -CONT "$process_pid" 2>/dev/null || true
            kill -INT "$process_pid" 2>/dev/null || true
            wait "$process_pid" 2>/dev/null || true
        fi
    done

    exec 9>&- 2>/dev/null || true

    if [[ "$TEST_TMP_DIR" == /tmp/pubsub-tests-* ]] \
        && [[ -d "$TEST_TMP_DIR" ]]; then
        rm -rf -- "$TEST_TMP_DIR"
    fi
}

trap cleanup EXIT

fail()
{
    printf 'Тест не пройден: %s\n' "$1" >&2
    exit 1
}

pass()
{
    PASSED_TESTS=$((PASSED_TESTS + 1))
    printf '\n[УСПЕХ] Тест %d пройден: %s\n' \
        "$PASSED_TESTS" "$1"
}

# Отделяет очередной тест от предыдущего заметным заголовком.
start_test()
{
    local test_number=$1
    local test_name=$2

    printf '\n============================================================\n'
    printf 'ТЕСТ %s: %s\n' "$test_number" "$test_name"
    printf '============================================================\n'
}

# Показывает сохранённый вывод одной из проверяемых программ.
show_program_output()
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

# Показывает только важные строки длинного журнала брокера.
show_selected_output()
{
    local title=$1
    local log_file=$2
    local pattern=$3

    printf '%s\n' "$title"
    grep -E "$pattern" "$log_file" 2>/dev/null \
        | sed 's/^/  /' \
        || true
}

wait_for_text()
{
    local file_name=$1
    local expected_text=$2

    for ((attempt = 0; attempt < 50; ++attempt)); do
        if grep -Fq "$expected_text" "$file_name" 2>/dev/null; then
            return 0
        fi

        sleep 0.1
    done

    return 1
}

wait_for_text_count()
{
    local file_name=$1
    local expected_text=$2
    local expected_count=$3

    for ((attempt = 0; attempt < 50; ++attempt)); do
        local actual_count
        actual_count=$(grep -Fc "$expected_text" "$file_name" 2>/dev/null || true)

        if ((actual_count >= expected_count)); then
            return 0
        fi

        sleep 0.1
    done

    return 1
}

queue_exists()
{
    ipcs -q | awk -v key="$QUEUE_KEY" '$1 == key { found = 1 } END { exit !found }'
}

start_broker()
{
    local log_file=$1

    stdbuf -oL -eL "$PROGRAM" -b >"$log_file" 2>&1 &
    BROKER_PID=$!

    wait_for_text "$log_file" "Очередь создана" \
        || fail "брокер не создал очередь"
}

# Переходим в папку задания, чтобы программа запускалась как ./pubsub.
# Поэтому в подсказке по использованию не выводится полный путь к файлу.
cd "$PROJECT_DIR" || fail "не удалось открыть папку задания"

if [[ ! -x "$PROGRAM" ]]; then
    fail "исполняемый файл pubsub не найден"
fi

if queue_exists; then
    fail "перед тестированием заверши уже работающий брокер"
fi

# Тест 1: программа должна отклонять запуск без указания роли.
start_test 1 "Запуск без аргументов"

if "$PROGRAM" >"$TEST_TMP_DIR/no_arguments.log" 2>&1; then
    fail "запуск без аргументов завершился успешно"
fi

grep -Fq "не указана роль" "$TEST_TMP_DIR/no_arguments.log" \
    || fail "нет сообщения об отсутствующей роли"

show_program_output "Вывод программы:" \
    "$TEST_TMP_DIR/no_arguments.log"

pass "запуск без аргументов"

# Тест 2: неизвестный ключ командной строки должен быть отклонён.
start_test 2 "Неизвестный ключ"

if "$PROGRAM" -x >"$TEST_TMP_DIR/unknown_option.log" 2>&1; then
    fail "неизвестный ключ был принят"
fi

grep -Fq "неизвестный ключ" "$TEST_TMP_DIR/unknown_option.log" \
    || fail "нет сообщения о неизвестном ключе"

show_program_output "Вывод программы:" \
    "$TEST_TMP_DIR/unknown_option.log"

pass "неизвестный ключ"

# Тест 3: каждая роль должна отклонять неправильное количество аргументов.
start_test 3 "Неправильные аргументы ролей"

if "$PROGRAM" -b extra_argument \
    >"$TEST_TMP_DIR/broker_extra_argument.log" 2>&1; then
    fail "брокер принял лишний аргумент"
fi

if "$PROGRAM" -p \
    >"$TEST_TMP_DIR/publisher_without_topic.log" 2>&1; then
    fail "издатель запустился без темы"
fi

if "$PROGRAM" -s \
    >"$TEST_TMP_DIR/subscriber_without_topic.log" 2>&1; then
    fail "подписчик запустился без темы"
fi

grep -Fq "дополнительные аргументы не нужны" \
    "$TEST_TMP_DIR/broker_extra_argument.log" \
    || fail "брокер не объяснил ошибку в аргументах"

grep -Fq "издателю нужно указать одну тему" \
    "$TEST_TMP_DIR/publisher_without_topic.log" \
    || fail "издатель не объяснил отсутствие темы"

grep -Fq "подписчику нужно указать хотя бы одну тему" \
    "$TEST_TMP_DIR/subscriber_without_topic.log" \
    || fail "подписчик не объяснил отсутствие темы"

show_program_output "Брокер с лишним аргументом:" \
    "$TEST_TMP_DIR/broker_extra_argument.log"
show_program_output "Издатель без темы:" \
    "$TEST_TMP_DIR/publisher_without_topic.log"
show_program_output "Подписчик без темы:" \
    "$TEST_TMP_DIR/subscriber_without_topic.log"

pass "неправильные аргументы ролей"

# Тест 4: издатель и подписчик не должны запускаться без брокера.
start_test 4 "Запуск участников без брокера"

if printf 'Новость\n' | "$PROGRAM" -p sport \
    >"$TEST_TMP_DIR/publisher_without_broker.log" 2>&1; then
    fail "издатель запустился без брокера"
fi

if "$PROGRAM" -s sport \
    >"$TEST_TMP_DIR/subscriber_without_broker.log" 2>&1; then
    fail "подписчик запустился без брокера"
fi

grep -Fq "брокер ещё не запущен" \
    "$TEST_TMP_DIR/publisher_without_broker.log" \
    || fail "издатель не сообщил об отсутствии брокера"

grep -Fq "брокер ещё не запущен" \
    "$TEST_TMP_DIR/subscriber_without_broker.log" \
    || fail "подписчик не сообщил об отсутствии брокера"

show_program_output "Издатель без брокера:" \
    "$TEST_TMP_DIR/publisher_without_broker.log"
show_program_output "Подписчик без брокера:" \
    "$TEST_TMP_DIR/subscriber_without_broker.log"

pass "участники без брокера"

start_broker "$TEST_TMP_DIR/broker.log"

# Тест 5: при существующей очереди второй брокер должен завершиться с ошибкой.
start_test 5 "Защита от запуска второго брокера"

if "$PROGRAM" -b >"$TEST_TMP_DIR/second_broker.log" 2>&1; then
    fail "второй брокер запустился одновременно с первым"
fi

grep -Fq "другой брокер уже работает" \
    "$TEST_TMP_DIR/second_broker.log" \
    || fail "второй брокер не вывел ожидаемую ошибку"

show_program_output "Вывод второго брокера:" \
    "$TEST_TMP_DIR/second_broker.log"

pass "защита от второго брокера"

# Тест 6: две публикации sport должны получить только подписчики темы sport.
start_test 6 "Маршрутизация сообщений по темам"

stdbuf -oL -eL "$PROGRAM" -s sport \
    >"$TEST_TMP_DIR/sport_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

stdbuf -oL -eL "$PROGRAM" -s programming \
    >"$TEST_TMP_DIR/programming_subscriber.log" 2>&1 &
SECOND_SUBSCRIBER_PID=$!

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=sport" \
    || fail "брокер не зарегистрировал подписку sport"

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=programming" \
    || fail "брокер не зарегистрировал подписку programming"

printf 'Первая новость\nВторая новость\n' \
    | "$PROGRAM" -p sport \
        >"$TEST_TMP_DIR/routing_publisher.log" 2>&1 \
    || fail "издатель не смог отправить публикации"

wait_for_text "$TEST_TMP_DIR/sport_subscriber.log" "Вторая новость" \
    || fail "подписчик sport не получил обе публикации"

if grep -Fq "Получено:" "$TEST_TMP_DIR/programming_subscriber.log"; then
    fail "подписчик programming получил публикацию sport"
fi

show_program_output "Вывод издателя sport:" \
    "$TEST_TMP_DIR/routing_publisher.log"
show_program_output "Вывод подписчика sport:" \
    "$TEST_TMP_DIR/sport_subscriber.log"
show_program_output "Вывод подписчика programming:" \
    "$TEST_TMP_DIR/programming_subscriber.log"

pass "маршрутизация по темам и несколько публикаций"

kill -INT "$SUBSCRIBER_PID"
kill -INT "$SECOND_SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" || fail "подписчик sport завершился с ошибкой"
wait "$SECOND_SUBSCRIBER_PID" \
    || fail "подписчик programming завершился с ошибкой"
SUBSCRIBER_PID=""
SECOND_SUBSCRIBER_PID=""

wait_for_text "$TEST_TMP_DIR/broker.log" "Удалена подписка" \
    || fail "брокер не обработал отписку"

# Тест 7: один подписчик должен получать сообщения по каждой теме,
# указанной в его аргументах запуска.
start_test 7 "Один подписчик и несколько тем"

stdbuf -oL -eL "$PROGRAM" -s multi_sport multi_programming \
    >"$TEST_TMP_DIR/multi_topic_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=multi_sport" \
    || fail "брокер не зарегистрировал первую тему подписчика"

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=multi_programming" \
    || fail "брокер не зарегистрировал вторую тему подписчика"

printf 'Сообщение о спорте\n' \
    | "$PROGRAM" -p multi_sport \
        >"$TEST_TMP_DIR/multi_sport_publisher.log" 2>&1 \
    || fail "не удалось отправить сообщение первой темы"

printf 'Сообщение о программировании\n' \
    | "$PROGRAM" -p multi_programming \
        >"$TEST_TMP_DIR/multi_programming_publisher.log" 2>&1 \
    || fail "не удалось отправить сообщение второй темы"

wait_for_text "$TEST_TMP_DIR/multi_topic_subscriber.log" \
    "Сообщение о спорте" \
    || fail "подписчик не получил сообщение первой темы"

wait_for_text "$TEST_TMP_DIR/multi_topic_subscriber.log" \
    "Сообщение о программировании" \
    || fail "подписчик не получил сообщение второй темы"

kill -INT "$SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" \
    || fail "подписчик нескольких тем завершился с ошибкой"
SUBSCRIBER_PID=""

show_program_output "Подписчик двух тем:" \
    "$TEST_TMP_DIR/multi_topic_subscriber.log"
show_program_output "Издатель темы multi_sport:" \
    "$TEST_TMP_DIR/multi_sport_publisher.log"
show_program_output "Издатель темы multi_programming:" \
    "$TEST_TMP_DIR/multi_programming_publisher.log"

pass "один подписчик и несколько тем"

# Тест 8: одну публикацию должны получить все подписчики её темы.
start_test 8 "Несколько подписчиков одной темы"

stdbuf -oL -eL "$PROGRAM" -s common_topic \
    >"$TEST_TMP_DIR/first_common_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

stdbuf -oL -eL "$PROGRAM" -s common_topic \
    >"$TEST_TMP_DIR/second_common_subscriber.log" 2>&1 &
SECOND_SUBSCRIBER_PID=$!

wait_for_text_count "$TEST_TMP_DIR/broker.log" "тема=common_topic" 2 \
    || fail "брокер не зарегистрировал двух подписчиков одной темы"

printf 'Общая публикация\n' \
    | "$PROGRAM" -p common_topic \
        >"$TEST_TMP_DIR/common_topic_publisher.log" 2>&1 \
    || fail "не удалось отправить общую публикацию"

wait_for_text "$TEST_TMP_DIR/first_common_subscriber.log" \
    "Общая публикация" \
    || fail "первый подписчик не получил общую публикацию"

wait_for_text "$TEST_TMP_DIR/second_common_subscriber.log" \
    "Общая публикация" \
    || fail "второй подписчик не получил общую публикацию"

kill -INT "$SUBSCRIBER_PID"
kill -INT "$SECOND_SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" || fail "первый общий подписчик завершился с ошибкой"
wait "$SECOND_SUBSCRIBER_PID" \
    || fail "второй общий подписчик завершился с ошибкой"
SUBSCRIBER_PID=""
SECOND_SUBSCRIBER_PID=""

show_program_output "Первый подписчик common_topic:" \
    "$TEST_TMP_DIR/first_common_subscriber.log"
show_program_output "Второй подписчик common_topic:" \
    "$TEST_TMP_DIR/second_common_subscriber.log"
show_program_output "Издатель common_topic:" \
    "$TEST_TMP_DIR/common_topic_publisher.log"

pass "несколько подписчиков одной темы"

# Тест 9: несколько издателей могут одновременно отправлять сообщения
# в одну очередь, а подписчик должен получить сообщения каждого из них.
start_test 9 "Несколько одновременных издателей"

stdbuf -oL -eL "$PROGRAM" -s publishers_topic \
    >"$TEST_TMP_DIR/multiple_publishers_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=publishers_topic" \
    || fail "брокер не зарегистрировал подписчика нескольких издателей"

printf 'Первый издатель\n' \
    | "$PROGRAM" -p publishers_topic \
        >"$TEST_TMP_DIR/first_publisher.log" 2>&1 &
PUBLISHER_PID=$!

printf 'Второй издатель\n' \
    | "$PROGRAM" -p publishers_topic \
        >"$TEST_TMP_DIR/second_publisher.log" 2>&1 &
SECOND_PUBLISHER_PID=$!

wait "$PUBLISHER_PID" || fail "первый издатель завершился с ошибкой"
wait "$SECOND_PUBLISHER_PID" || fail "второй издатель завершился с ошибкой"
PUBLISHER_PID=""
SECOND_PUBLISHER_PID=""

wait_for_text "$TEST_TMP_DIR/multiple_publishers_subscriber.log" \
    "Первый издатель" \
    || fail "не получено сообщение первого издателя"

wait_for_text "$TEST_TMP_DIR/multiple_publishers_subscriber.log" \
    "Второй издатель" \
    || fail "не получено сообщение второго издателя"

kill -INT "$SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" \
    || fail "подписчик нескольких издателей завершился с ошибкой"
SUBSCRIBER_PID=""

show_program_output "Первый издатель:" \
    "$TEST_TMP_DIR/first_publisher.log"
show_program_output "Второй издатель:" \
    "$TEST_TMP_DIR/second_publisher.log"
show_program_output "Подписчик двух издателей:" \
    "$TEST_TMP_DIR/multiple_publishers_subscriber.log"

pass "несколько одновременно работающих издателей"

# Тест 10: при Ctrl+C брокер должен завершить ожидающих участников сигналами.
start_test 10 "Общее завершение по SIGINT"

stdbuf -oL -eL "$PROGRAM" -s news \
    >"$TEST_TMP_DIR/shutdown_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

mkfifo "$TEST_TMP_DIR/publisher_input"

stdbuf -oL -eL "$PROGRAM" -p news \
    <"$TEST_TMP_DIR/publisher_input" \
    >"$TEST_TMP_DIR/shutdown_publisher.log" 2>&1 &
PUBLISHER_PID=$!

exec 9>"$TEST_TMP_DIR/publisher_input"

wait_for_text "$TEST_TMP_DIR/broker.log" \
    "Зарегистрирован издатель PID=$PUBLISHER_PID" \
    || fail "брокер не зарегистрировал постоянного издателя"

wait_for_text "$TEST_TMP_DIR/broker.log" "тема=news" \
    || fail "брокер не зарегистрировал подписчика news"

kill -INT "$BROKER_PID"

wait "$BROKER_PID" || fail "брокер завершился с ошибкой"
wait "$SUBSCRIBER_PID" || fail "подписчик не завершился по сигналу брокера"
wait "$PUBLISHER_PID" || fail "издатель не завершился по сигналу брокера"

exec 9>&-

BROKER_PID=""
SUBSCRIBER_PID=""
PUBLISHER_PID=""

grep -Fq "SIGINT отправлен издателю" "$TEST_TMP_DIR/broker.log" \
    || fail "брокер не отправил SIGINT издателю"

grep -Fq "SIGINT отправлен подписчику" "$TEST_TMP_DIR/broker.log" \
    || fail "брокер не отправил SIGINT подписчику"

grep -Fq "Все участники подтвердили завершение" \
    "$TEST_TMP_DIR/broker.log" \
    || fail "брокер не дождался подтверждений участников"

if queue_exists; then
    fail "очередь осталась после завершения брокера"
fi

show_selected_output "Завершение участников брокером:" \
    "$TEST_TMP_DIR/broker.log" \
    'SIGINT отправлен|подтвердили завершение|Очередь удалена'
show_program_output "Завершение подписчика:" \
    "$TEST_TMP_DIR/shutdown_subscriber.log"
show_program_output "Завершение издателя:" \
    "$TEST_TMP_DIR/shutdown_publisher.log"

pass "общее завершение и удаление очереди"

# Тест 11: если участник аварийно исчез, брокер должен удалить очередь по таймауту.
start_test 11 "Таймаут аварийно завершившегося участника"

start_broker "$TEST_TMP_DIR/timeout_broker.log"

stdbuf -oL -eL "$PROGRAM" -s timeout_topic \
    >"$TEST_TMP_DIR/timeout_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

wait_for_text "$TEST_TMP_DIR/timeout_broker.log" "timeout_topic" \
    || fail "брокер не зарегистрировал тестового подписчика"

kill -KILL "$SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" 2>/dev/null || true
SUBSCRIBER_PID=""

kill -INT "$BROKER_PID"
wait "$BROKER_PID" || fail "брокер завершился с ошибкой после таймаута"
BROKER_PID=""

grep -Fq "Таймаут ожидания участников истёк" \
    "$TEST_TMP_DIR/timeout_broker.log" \
    || fail "брокер не дождался таймаута"

if queue_exists; then
    fail "очередь осталась после таймаута"
fi

show_selected_output "Вывод брокера при аварийном завершении участника:" \
    "$TEST_TMP_DIR/timeout_broker.log" \
    'Добавлена подписка|SIGINT|Таймаут|Очередь удалена'

pass "таймаут аварийно завершившегося участника"

# Тест 12: брокер должен ждать таймаута, если участники завершились,
# но в очереди осталась непрочитанная публикация.
start_test 12 "Ожидание освобождения очереди"

start_broker "$TEST_TMP_DIR/pending_message_broker.log"

stdbuf -oL -eL "$PROGRAM" -s pending_topic \
    >"$TEST_TMP_DIR/pending_message_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

wait_for_text "$TEST_TMP_DIR/pending_message_broker.log" "pending_topic" \
    || fail "брокер не зарегистрировал подписчика pending_topic"

kill -STOP "$SUBSCRIBER_PID"

printf 'Непрочитанная публикация\n' \
    | "$PROGRAM" -p pending_topic \
        >"$TEST_TMP_DIR/pending_message_publisher.log" 2>&1 \
    || fail "не удалось создать непрочитанную публикацию"

wait_for_text "$TEST_TMP_DIR/pending_message_broker.log" \
    "Доставлено подписчикам: 1" \
    || fail "брокер не поместил публикацию в очередь"

kill -INT "$SUBSCRIBER_PID"
kill -CONT "$SUBSCRIBER_PID"
wait "$SUBSCRIBER_PID" \
    || fail "подписчик не завершился после отложенного SIGINT"
SUBSCRIBER_PID=""

wait_for_text "$TEST_TMP_DIR/pending_message_broker.log" \
    "Удалена подписка" \
    || fail "брокер не обработал отписку перед завершением"

kill -INT "$BROKER_PID"
wait "$BROKER_PID" \
    || fail "брокер завершился с ошибкой при непрочитанном сообщении"
BROKER_PID=""

grep -Fq "Таймаут ожидания участников истёк" \
    "$TEST_TMP_DIR/pending_message_broker.log" \
    || fail "брокер удалил непустую очередь без ожидания таймаута"

if queue_exists; then
    fail "непустая очередь осталась после таймаута"
fi

show_selected_output "Вывод брокера при непрочитанном сообщении:" \
    "$TEST_TMP_DIR/pending_message_broker.log" \
    'Добавлена подписка|Доставлено|Удалена подписка|Таймаут|Очередь удалена'
show_program_output "Вывод издателя непрочитанного сообщения:" \
    "$TEST_TMP_DIR/pending_message_publisher.log"

pass "ожидание освобождения очереди"

# Тест 13: издатель и подписчик должны сами завершиться, если брокер
# аварийно исчез, а его очередь была удалена без рассылки SIGINT.
start_test 13 "Завершение при недоступной очереди"

start_broker "$TEST_TMP_DIR/unavailable_queue_broker.log"

stdbuf -oL -eL "$PROGRAM" -s unavailable_topic \
    >"$TEST_TMP_DIR/unavailable_queue_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

mkfifo "$TEST_TMP_DIR/unavailable_publisher_input"

stdbuf -oL -eL "$PROGRAM" -p unavailable_topic \
    <"$TEST_TMP_DIR/unavailable_publisher_input" \
    >"$TEST_TMP_DIR/unavailable_queue_publisher.log" 2>&1 &
PUBLISHER_PID=$!

exec 9>"$TEST_TMP_DIR/unavailable_publisher_input"

wait_for_text "$TEST_TMP_DIR/unavailable_queue_broker.log" \
    "Зарегистрирован издатель" \
    || fail "брокер не зарегистрировал издателя для проверки очереди"

wait_for_text "$TEST_TMP_DIR/unavailable_queue_broker.log" \
    "unavailable_topic" \
    || fail "брокер не зарегистрировал подписчика для проверки очереди"

queue_id=$(
    ipcs -q \
        | awk -v key="$QUEUE_KEY" '$1 == key { print $2; exit }'
)

[[ -n "$queue_id" ]] \
    || fail "не найден ID очереди для проверки её удаления"

kill -KILL "$BROKER_PID"
wait "$BROKER_PID" 2>/dev/null || true
BROKER_PID=""

ipcrm -q "$queue_id" \
    || fail "не удалось удалить тестовую очередь"

wait_for_text "$TEST_TMP_DIR/unavailable_queue_subscriber.log" \
    "Очередь брокера недоступна" \
    || fail "подписчик не обнаружил удаление очереди"

wait_for_text "$TEST_TMP_DIR/unavailable_queue_publisher.log" \
    "Очередь брокера недоступна" \
    || fail "издатель не обнаружил удаление очереди"

wait "$SUBSCRIBER_PID" \
    || fail "подписчик завершился с ошибкой после удаления очереди"
wait "$PUBLISHER_PID" \
    || fail "издатель завершился с ошибкой после удаления очереди"

exec 9>&-

SUBSCRIBER_PID=""
PUBLISHER_PID=""

if queue_exists; then
    fail "очередь осталась после проверки недоступности"
fi

show_program_output "Подписчик после удаления очереди:" \
    "$TEST_TMP_DIR/unavailable_queue_subscriber.log"
show_program_output "Издатель после удаления очереди:" \
    "$TEST_TMP_DIR/unavailable_queue_publisher.log"

pass "завершение участников при недоступной очереди"

printf '\n============================================================\n'
printf 'ВСЕ ТЕСТЫ ПРОЙДЕНЫ: %d\n' "$PASSED_TESTS"
printf '============================================================\n'
