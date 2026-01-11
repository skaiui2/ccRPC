#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "services_fs.h"
#include <stdio.h>
#include "rpc_gen.h"
#include <unistd.h>


void testAB(void) 
{
    printf("NodeA: starting...\n");

    rpc_init();
    rpc_register_all();   // NodeA 只提供 fs_read

    rpc_tcp_ctx_t listener, client;
    rpc_tcp_listen(&listener, 9001);
    printf("NodeA: listening on 9001...\n");

    rpc_tcp_accept(&listener, &client);
    printf("NodeA: client accepted\n");

    rpc_transport_t t = {
        .send = rpc_tcp_send,
        .recv = rpc_tcp_recv,
        .user = &client
    };
    rpc_set_transport(&t);

    while (1) {
        rpc_poll();
    }
}


void testBA(void) 
{
    printf("NodeA: starting...\n");

    rpc_init();
    rpc_register_all();   // NodeA 提供 fs_read

    rpc_tcp_ctx_t ctx;
    rpc_tcp_connect(&ctx, "127.0.0.1", 9001);
    printf("NodeA: connected to NodeB\n");

    rpc_transport_t t = {
        .send = rpc_tcp_send,
        .recv = rpc_tcp_recv,
        .user = &ctx
    };
    rpc_set_transport(&t);

    sleep(1);

    struct rpc_param_shell_exec p = {
        .cmd = "echo hello-from-NodeA"
    };
    struct rpc_result_shell_exec r;

    int st = rpc_call_shell_exec(&p, &r);
    printf("NodeA: rpc_call_shell_exec() => %d\n", st);
    if (st == 0) {
        printf("NodeA: output=%s exitcode=%u\n",
               r.output, (unsigned)r.exitcode);
    }

    while (1) {
        rpc_poll();
    }
}

int main(void)
{
    testAB();
}
