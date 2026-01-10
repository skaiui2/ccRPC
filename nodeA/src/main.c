#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "services_fs.h"
#include <stdio.h>
#include "rpc_result.h"
#include "rpc_methods_decl.h"


int main() {
    printf("Node A starting...\n");

    rpc_init();
    rpc_register_fs_read();

    rpc_tcp_ctx_t listener, client;
    rpc_tcp_listen(&listener, 9001);
    rpc_tcp_accept(&listener, &client);

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
