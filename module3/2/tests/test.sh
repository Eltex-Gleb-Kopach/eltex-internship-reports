#!/usr/bin/env bash

set -u

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
PROGRAM="$PROJECT_DIR/pubsub"
QUEUE_KEY="0x454c5458"
TEST_TMP_DIR=$(mktemp -d /tmp/pubsub-tests-XXXXXX)

BROKER_PID=""
SUBSCRIBER_PID=""
SECOND_SUBSCRIBER_PID=""
PUBLISHER_PID=""
PASSED_TESTS=0

cleanup()
{
    for process_pid in \
        "$PUBLISHER_PID" \
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
    printf 'Тест %d пройден: %s\n' "$PASSED_TESTS" "$1"
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

if [[ ! -x "$PROGRAM" ]]; then
    fail "исполняемый файл pubsub не найден"
fi

if queue_exists; then
    fail "перед тестированием заверши уже работающий брокер"
fi

# Тест 1: программа должна отклонять запуск без указания роли.
if "$PROGRAM" >"$TEST_TMP_DIR/no_arguments.log" 2>&1; then
    fail "запуск без аргументов завершился успешно"
fi

grep -Fq "не указана роль" "$TEST_TMP_DIR/no_arguments.log" \
    || fail "нет сообщения об отсутствующей роли"

pass "запуск без аргументов"

# Тест 2: неизвестный ключ командной строки должен быть отклонён.
if "$PROGRAM" -x >"$TEST_TMP_DIR/unknown_option.log" 2>&1; then
    fail "неизвестный ключ был принят"
fi

grep -Fq "неизвестный ключ" "$TEST_TMP_DIR/unknown_option.log" \
    || fail "нет сообщения о неизвестном ключе"

pass "неизвестный ключ"

# Тест 3: издатель и подписчик не должны запускаться без брокера.
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

pass "участники без брокера"

start_broker "$TEST_TMP_DIR/broker.log"

# Тест 4: при существующей очереди второй брокер должен завершиться с ошибкой.
if "$PROGRAM" -b >"$TEST_TMP_DIR/second_broker.log" 2>&1; then
    fail "второй брокер запустился одновременно с первым"
fi

grep -Fq "другой брокер уже работает" \
    "$TEST_TMP_DIR/second_broker.log" \
    || fail "второй брокер не вывел ожидаемую ошибку"

pass "защита от второго брокера"

# Тест 5: две публикации sport должны получить только подписчики темы sport.
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

# Тест 6: при Ctrl+C брокер должен завершить ожидающих участников сигналами.
stdbuf -oL -eL "$PROGRAM" -s news \
    >"$TEST_TMP_DIR/shutdown_subscriber.log" 2>&1 &
SUBSCRIBER_PID=$!

mkfifo "$TEST_TMP_DIR/publisher_input"

stdbuf -oL -eL "$PROGRAM" -p news \
    <"$TEST_TMP_DIR/publisher_input" \
    >"$TEST_TMP_DIR/shutdown_publisher.log" 2>&1 &
PUBLISHER_PID=$!

exec 9>"$TEST_TMP_DIR/publisher_input"

wait_for_text "$TEST_TMP_DIR/broker.log" "Зарегистрирован издатель" \
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

pass "общее завершение и удаление очереди"

# Тест 7: если участник аварийно исчез, брокер должен удалить очередь по таймауту.
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

pass "таймаут аварийно завершившегося участника"

# Тест 8: брокер должен ждать таймаута, если участники завершились,
# но в очереди осталась непрочитанная публикация.
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

pass "ожидание освобождения очереди"

# Тест 9: издатель и подписчик должны сами завершиться, если брокер
# аварийно исчез, а его очередь была удалена без рассылки SIGINT.
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

pass "завершение участников при недоступной очереди"

printf 'Все тесты пройдены: %d.\n' "$PASSED_TESTS"
