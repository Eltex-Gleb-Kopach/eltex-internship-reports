#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

/*
 * Запускает подписчика.
 * topic_count — количество тем.
 * topics — массив строк с названиями тем.
 */
int run_subscriber(int topic_count, char *topics[]);

#endif