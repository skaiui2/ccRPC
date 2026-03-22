#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "rpc.h"
#include "rpc_gen.h"
#include "rpc_cal_tcp.h"

static rpc_tcp_ctx_t tcp_ctx;
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
        ssize_t n = rpc_tcp_recv(&tcp_ctx, buf, sizeof(buf));
        if (n > 0)
            rpc_on_data(tAtoB, buf, n);
        usleep(100);
    }
    return NULL;
}

static void *call_thread(void *arg)
{
    const char *cmds[] = {
        "echo hello",
        "id",
        "whoami"
    };
    size_t cmd_count = sizeof(cmds)/sizeof(cmds[0]);
    uint64_t i = 0;

    sleep(1);

    for (;;) {
        const char *cmd = cmds[rand() % cmd_count];

        FILE *fp = popen(cmd, "r");
        if (!fp) _exit(1);

        char local_buf[4096];
        size_t n = fread(local_buf, 1, sizeof(local_buf)-1, fp);
        local_buf[n] = 0;
        int local_exit = pclose(fp);

        uint32_t local_crc = crc32_calc((uint8_t*)local_buf, n);

        struct rpc_param_shell_exec p = { .cmd = (char*)cmd };
        struct rpc_result_shell_exec r;

        int st = rpc_call_shell_exec(&p, &r, 10000);
        if (st != 0) {  
            heap_get_stats(); 
            printf("ST: %d\r\n", st);
            _exit(1);
        }

        size_t rn = strlen(r.output ? r.output : "");
        uint32_t remote_crc = crc32_calc((uint8_t*)r.output, rn);

        if (local_crc != remote_crc || local_exit != (int)r.exitcode) {
            printf("SHELL FAIL iter=%llu cmd=%s\n", i, cmd);
            printf("local_crc=%08X remote_crc=%08X\n", local_crc, remote_crc);
            printf("local_exit=%d remote_exit=%u\n", local_exit, r.exitcode);
            _exit(1);
        }

        if ((i++ % 1000) == 0)
            printf("SHELL PASS iter=%llu cmd=%s local=%08X remote=%08X\n",
                   i, cmd, local_crc, remote_crc);

        free_result_shell_exec(&r);
    }

    return NULL;
}

int main(void)
{
    srand(time(NULL));

    if (rpc_tcp_connect(&tcp_ctx, "127.0.0.1", 9001) < 0)
        return -1;

    rpc_init(16, 16, 4);
    rpc_register_all();

    tAtoB = rpc_trans_class_create(
        rpc_tcp_send,
        rpc_tcp_recv,
        rpc_tcp_close,
        &tcp_ctx
    );

    rpc_bind_transport("fs.read", tAtoB);
    rpc_bind_transport("shell.exec", tAtoB);

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    pthread_join(th_call, NULL);
    return 0;
}
