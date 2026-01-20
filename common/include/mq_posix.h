#ifndef MQ_POSIX_H
#define MQ_POSIX_H

#include <stddef.h>
#include <mqueue.h>

int mqposix_open_sender(const char *name);
int mqposix_open_receiver(const char *name);

int mqposix_send(int mq, const void *buf, size_t len);
int mqposix_recv(int mq, void *buf, size_t maxlen);

#endif
