#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "rpc_tlv.h"
#include "services_shell.h"
#include "rpc_gen.h"
#include "rpc_cal_tcp.h"
#include <stdio.h>
#include <unistd.h>
#include <memory.h>


#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "rpc_gen.h"
#include <stdio.h>
#include <unistd.h>

void testAB(void) 
{
    printf("NodeB: starting...\n");

    rpc_init();
    rpc_register_all();   // NodeB 提供 shell_exec

    rpc_tcp_ctx_t ctx;
    rpc_tcp_connect(&ctx, "127.0.0.1", 9001);
    printf("NodeB: connected to NodeA\n");

    rpc_transport_t t = {
        .send = rpc_tcp_send,
        .recv = rpc_tcp_recv,
        .user = &ctx
    };
    rpc_set_transport(&t);

    sleep(1);

    struct rpc_param_fs_read p = {
        .path = "/etc/config",
        .offset = 0,
        .size = 128
    };
    struct rpc_result_fs_read r;

    int st = rpc_call_fs_read(&p, &r);
    printf("NodeB: rpc_call_fs_read() => %d\n", st);
    if (st == 0) {
        printf("NodeB: len=%u eof=%u\n", (unsigned)r.len, (unsigned)r.eof);
        printf("NodeB: data=%.*s\n", (int)r.data.len, (char*)r.data.ptr);
    }

    while (1) {
        rpc_poll();
    }
}



void tsetBA(void) {
    printf("NodeB: starting...\n");

    rpc_init();
    rpc_register_all();   // NodeB 提供 shell_exec

    rpc_tcp_ctx_t listener, client;
    rpc_tcp_listen(&listener, 9001);
    printf("NodeB: listening on 9001...\n");

    rpc_tcp_accept(&listener, &client);
    printf("NodeB: client accepted\n");

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

int main(void)
{
    testAB();
}