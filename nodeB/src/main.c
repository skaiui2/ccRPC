#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "rpc.h"
#include "rpc_gen.h"
#include "rpc_cal_tcp.h"

static rpc_tcp_ctx_t listener;
static rpc_tcp_ctx_t client;
static struct rpc_transport_class *tAtoB;

static uint32_t crc32_calc(const uint8_t *buf, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

static void *poll_thread(void *arg)
{
    uint8_t buf[4096];
    for (;;) {
        ssize_t n = rpc_tcp_recv(&client, buf, sizeof(buf));
        if (n > 0)
            rpc_on_data(tAtoB, buf, n);
        usleep(100);
    }
    return NULL;
}

static void *call_thread(void *arg)
{
    const char *path = "./big.bin";
    const size_t MAX_CHUNK = 1024;
    uint64_t i = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) _exit(1);

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    uint8_t local_buf[MAX_CHUNK];

    sleep(1);

    for (;;) {
        size_t sz = 1 + (rand() % MAX_CHUNK);
        long max_off = fsize - (long)sz;
        long off = (max_off > 0) ? (rand() % max_off) : 0;

        fseek(fp, off, SEEK_SET);
        fread(local_buf, 1, sz, fp);
        uint32_t local_crc = crc32_calc(local_buf, sz);

        struct rpc_param_fs_read p = {
            .path   = (char*)path,
            .offset = (uint32_t)off,
            .size   = (uint32_t)sz
        };
        struct rpc_result_fs_read r;

        int st = rpc_call_fs_read(&p, &r, 10000);
        if (st != 0) {
            heap_get_stats();
            printf("ST: %d\r\n", st);
            _exit(1);
        }

        uint32_t remote_crc = crc32_calc(r.data.ptr, r.data.len);

        if (local_crc != remote_crc) {
            printf("FS FAIL iter=%llu off=%ld size=%zu local=%08X remote=%08X\n",
                   i, off, sz, local_crc, remote_crc);
            _exit(1);
        }

        if ((i++ % 1000) == 0)
            printf("FS PASS iter=%llu off=%ld size=%zu local=%08X remote=%08X\n",
                   i, off, sz, local_crc, remote_crc);

        free_result_fs_read(&r);
    }

    return NULL;
}

int main(void)
{
    srand(time(NULL));

    if (rpc_tcp_listen(&listener, 9001) < 0)
        return -1;

    if (rpc_tcp_accept(&listener, &client) < 0)
        return -1;

    rpc_init(16, 16, 4);
    rpc_register_all();

    tAtoB = rpc_trans_class_create(
        rpc_tcp_send,
        rpc_tcp_recv,
        rpc_tcp_close,
        &client
    );

    rpc_bind_transport("fs.read", tAtoB);
    rpc_bind_transport("shell.exec", tAtoB);

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    pthread_join(th_call, NULL);
    return 0;
}
