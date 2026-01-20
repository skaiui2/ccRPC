#include <mqueue.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "mq_posix.h"

// 严格在内核默认限制内：msg_max=10, msgsize_max=8192
static struct mq_attr mqattr = {
    .mq_flags   = 0,
    .mq_maxmsg  = 10,     // ≤ /proc/sys/fs/mqueue/msg_max
    .mq_msgsize = 256,   // ≤ /proc/sys/fs/mqueue/msgsize_max
    .mq_curmsgs = 0
};

int mqposix_open_sender(const char *name)
{
    mqd_t mq = mq_open(name, O_WRONLY | O_CREAT, 0666, &mqattr);
    if (mq == (mqd_t)-1) {
        perror(name);
        return -1;
    }
    return mq;
}

int mqposix_open_receiver(const char *name)
{
    mqd_t mq = mq_open(name, O_RDONLY | O_CREAT | O_NONBLOCK, 0666, &mqattr);
    if (mq == (mqd_t)-1) {
        perror(name);
        return -1;
    }
    return mq;
}


int mqposix_send(int mq, const void *buf, size_t len)
{
    if (mq < 0) {
        fprintf(stderr, "mqposix_send: bad mq=%d\n", mq);
        return -1;
    }
    if (mq_send(mq, buf, len, 0) < 0) {
        perror("mq_send");
        return -1;
    }
    return (int)len;
}

int mqposix_recv(int mq, void *buf, size_t maxlen)
{
    if (mq < 0) {
        fprintf(stderr, "mqposix_recv: bad mq=%d\n", mq);
        errno = EBADF;
        perror("mq_receive");
        return -1;
    }

    ssize_t n = mq_receive(mq, buf, maxlen, NULL);
    if (n < 0) {
        if (errno == EAGAIN) {
            return 0;   // 队列空，非错误
        }
        perror("mq_receive");
        return -1;
    }
    return (int)n;
}
