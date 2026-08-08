#define _POSIX_C_SOURCE 200809L

#include "signal_control.h"

#include <signal.h>
#include <stddef.h>

static volatile sig_atomic_t stop_requested = 0;

/* При получении SIGINT устанавливает флаг завершения. */
static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

int install_stop_signal_handler(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, NULL);
}

int is_stop_requested(void)
{
    return stop_requested != 0;
}