#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "services_shell.h"
#include "rpc_gen.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static struct rpc_transport_class *tAtoB;
static struct rpc_transport_class *tBtoA;

static void *poll_thread(void *arg)
{
    (void)arg;
    for (;;) rpc_poll();
    return NULL;
}

static void *call_thread(void *arg)
{
    (void)arg;
    sleep(1);

    struct rpc_param_fs_read p = {
        .path   = "/etc/config",
        .offset = 0,
        .size   = 128
    };
    struct rpc_result_fs_read r;

    int st = rpc_call_fs_read(&p, &r);
    printf("NodeB: fs.read => %d\n", st);
    if (st == 0) {
        printf("NodeB: len=%u eof=%u\n", (unsigned)r.len, (unsigned)r.eof);
        printf("NodeB: data=%.*s\n", (int)r.data.len, (char*)r.data.ptr);
    }

    return NULL;
}

int main(void)
{
    rpc_init();
    rpc_register_all();

    rpc_tcp_ctx_t ctx;
    rpc_tcp_connect(&ctx, "127.0.0.1", 9001);

    tBtoA = rpc_trans_class_alloc(rpc_tcp_send, rpc_tcp_recv, rpc_tcp_close, &ctx);

    rpc_register_transport(tBtoA);
    rpc_transport_register("fs.read", tBtoA);

    rpc_tcp_ctx_t listener, client;
    rpc_tcp_listen(&listener, 9002);
    rpc_tcp_accept(&listener, &client);

    tAtoB = rpc_trans_class_alloc(rpc_tcp_send, rpc_tcp_recv, rpc_tcp_close, &client);

    rpc_register_transport(tAtoB);
    rpc_transport_register("shell.exec", tAtoB);

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    pthread_join(th_call, NULL);
    return 0;
}
