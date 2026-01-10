#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "rpc_tlv.h"
#include "services_shell.h"
#include <stdio.h>
#include <unistd.h>
#include "rpc_methods_decl.h"

int main() {
    printf("Node B starting...\n");

    rpc_init();
    rpc_register_shell_exec();

    rpc_tcp_ctx_t ctx;
    rpc_tcp_connect(&ctx, "127.0.0.1", 9001);

    rpc_transport_t t = {
        .send = rpc_tcp_send,
        .recv = rpc_tcp_recv,
        .user = &ctx
    };
    rpc_set_transport(&t);

    sleep(1);

    uint8_t tlvbuf[256];
    size_t off = 0;

    off += tlv_write_string(&tlvbuf[off], "/etc/config");
    off += tlv_write_u32(&tlvbuf[off], 0);
    off += tlv_write_u32(&tlvbuf[off], 128);

    uint8_t resp[512]; 
    size_t resp_len = 0; 
    rpc_call_with_tlv("fs.read", tlvbuf, off, resp, &resp_len); 
    printf("resp TLV type=%u len=%zu\n", resp[0], resp_len);

    while (1) {
        rpc_poll();
    }
}
