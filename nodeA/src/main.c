#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "services_fs.h"
#include "rpc_gen.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static rpc_transport_t tAtoB;
static rpc_transport_t tBtoA;

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

    struct rpc_param_shell_exec p = { .cmd = "echo hello-from-NodeA" };
    struct rpc_result_shell_exec r;

    int st = rpc_call_shell_exec(&p, &r);
    printf("NodeA: shell.exec => %d\n", st);
    if (st == 0)
        printf("NodeA: output=%s exit=%u\n", r.output, r.exitcode);

    return NULL;
}

int main(void)
{
    rpc_init();
    rpc_register_all();

    rpc_tcp_ctx_t listener, client;
    rpc_tcp_listen(&listener, 9001);
    rpc_tcp_accept(&listener, &client);

    tBtoA.send  = rpc_tcp_send;
    tBtoA.recv  = rpc_tcp_recv;
    tBtoA.close = rpc_tcp_close;
    tBtoA.user  = &client;

    rpc_register_transport(&tBtoA);
    rpc_transport_register("fs.read", &tBtoA);

    rpc_tcp_ctx_t ctx;
    rpc_tcp_connect(&ctx, "127.0.0.1", 9002);

    tAtoB.send  = rpc_tcp_send;
    tAtoB.recv  = rpc_tcp_recv;
    tAtoB.close = rpc_tcp_close;
    tAtoB.user  = &ctx;

    rpc_register_transport(&tAtoB);
    rpc_transport_register("shell.exec", &tAtoB);

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    pthread_join(th_call, NULL);
    return 0;
}
