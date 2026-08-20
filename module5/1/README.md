## Среда выполнения

- ОС: Ubuntu 26.04 LTS
- Версия ядра: `7.0.0-29-generic`
- Архитектура: `x86_64`
- Компилятор: GCC 15.2.0

## Состав проекта

- `hello.c` — исходный код модуля;
- `Makefile` — сборка модуля через систему сборки ядра;

## Сборка

```bash
make
```

Очистка результатов сборки:

```bash
make clean
```

## Загрузка и проверка

Загрузка модуля:

```bash
sudo insmod ./hello.ko
```

Проверка наличия модуля:

```bash
lsmod | grep '^hello'
```

Просмотр сообщений:

```bash
sudo dmesg | grep 'hello:' | tail -n 10
```

## Выгрузка

```bash
sudo rmmod hello
```

Проверка выгрузки и просмотр журнала:

```bash
lsmod | grep '^hello' || echo "Модуль hello выгружен"
sudo dmesg | grep 'hello:' | tail -n 10
```
