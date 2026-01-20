#ifndef RPC_TIME_H
#define RPC_TIME_H
#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>

#define RPC_TIMEOUT_MS 5000

static inline uint64_t rpc_now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


#endif