#include "rpc_port.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

struct rpc_waiter {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int signaled;
};

static pthread_mutex_t g_rpc_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

void heap_lock(void)
{
    pthread_mutex_lock(&mem_lock);
}

void heap_unlock(void)
{
    pthread_mutex_unlock(&mem_lock);
}

void rpc_port_lock(void)
{
    pthread_mutex_lock(&g_rpc_lock);
}

void rpc_port_unlock(void)
{
    pthread_mutex_unlock(&g_rpc_lock);
}

struct rpc_waiter *rpc_waiter_create(void)
{
    struct rpc_waiter *w = malloc(sizeof(*w));
    if (!w) return NULL;

    pthread_mutex_init(&w->lock, NULL);
    pthread_cond_init(&w->cond, NULL);
    w->signaled = 0;
    return w;
}

void rpc_waiter_destroy(struct rpc_waiter *w)
{
    if (!w) return;
    pthread_mutex_destroy(&w->lock);
    pthread_cond_destroy(&w->cond);
    free(w);
}

int rpc_waiter_wait(struct rpc_waiter *w, uint32_t timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&w->lock);

    while (!w->signaled) {
        int r = pthread_cond_timedwait(&w->cond, &w->lock, &ts);
        if (r != 0) {
            pthread_mutex_unlock(&w->lock);
            return -1;
        }
    }

    w->signaled = 0;
    pthread_mutex_unlock(&w->lock);
    return 0;
}

void rpc_waiter_wake(struct rpc_waiter *w)
{
    pthread_mutex_lock(&w->lock);
    w->signaled = 1;
    pthread_cond_signal(&w->cond);
    pthread_mutex_unlock(&w->lock);
}

uint32_t rpc_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
