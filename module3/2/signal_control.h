#ifndef SIGNAL_CONTROL_H
#define SIGNAL_CONTROL_H

/* Настраивает завершение процесса по Ctrl+C. */
int install_stop_signal_handler(void);

/* Возвращает 1, если процесс получил SIGINT. */
int is_stop_requested(void);

#endif