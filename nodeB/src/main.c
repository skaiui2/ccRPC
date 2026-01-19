#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ccnet.h"
#include "cal_udp.h"

static cal_udp_ctx_t udpB;
static struct sockaddr_in fromC;   // C -> B 来源

int B_send(void *user, void *buf, size_t len)
{
    return 0; // B 不需要发送
}

int B_recv(void *user, void *buf, size_t maxlen)
{
    return cal_udp_recv(&udpB, buf, maxlen, &fromC);
}

int B_close(void *user)
{
    cal_udp_close(&udpB);
    return 0;
}

int main(void)
{
    printf("[B] 启动\n");

    if (cal_udp_open(&udpB, "0.0.0.0", 9003) < 0) {
        perror("udp_open");
        return -1;
    }

    ccnet_init(2, 16);

    struct ccnet_transport_class *ctc =
        ccnet_tran_class_alloc(B_send, B_recv, B_close, NULL);
    ccnet_trans_class_register(2, ctc);   // 本地递交 key=src=2

    // 全局拓扑
    ccnet_graph_set_edge(0, 1, 1);
    ccnet_graph_set_edge(1, 2, 1);
    ccnet_recompute_effective_graph();

    printf("[B] 等待 C 的数据...\n");

    uint8_t buf[2048];
    while (1) {
        int n = B_recv(NULL, buf, sizeof(buf));
        if (n > 0) {
            printf("[B] 收到来自 C 的 UDP 包，len=%d\n", n);
            ccnet_input(NULL, buf, n);
        }
    }

    return 0;
}
