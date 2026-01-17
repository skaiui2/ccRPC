#include <time.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "scp_time.h"
#include <pthread.h>
#include "scp.h"

static scp_timer_task g_task = NULL;

static void *scp_time_thread(void *arg)
{
    int tfd = timerfd_create(1, 0);

    struct itimerspec its;
    memset(&its, 0, sizeof(its));

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 20 * 1000 * 1000;  // 1ms

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1 * 1000 * 1000;

    timerfd_settime(tfd, 0, &its, NULL);

    uint64_t exp;

    while (1) {
        read(tfd, &exp, sizeof(exp));  
        if (g_task)
            g_task();               
    }

    return NULL;
}

int scp_time_init()
{
    g_task = scp_timer_process;

    pthread_t tid;
    pthread_create(&tid, NULL, scp_time_thread, NULL);
    pthread_detach(tid);

    return 0;
}
