#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ccnet.h"
#include "cal_udp.h"

static cal_udp_ctx_t udpC;
static struct sockaddr_in peerB;   // C -> B
static struct sockaddr_in fromA;   // A -> C 来源

int C_send(void *user, void *buf, size_t len)
{
    return cal_udp_send(&udpC, buf, len, &peerB);
}

int C_recv(void *user, void *buf, size_t maxlen)
{
    return cal_udp_recv(&udpC, buf, maxlen, &fromA);
}

int C_close(void *user)
{
    cal_udp_close(&udpC);
    return 0;
}

int main(void)
{
    printf("[C] 启动\n");

    if (cal_udp_open(&udpC, "0.0.0.0", 9002) < 0) {
        perror("udp_open");
        return -1;
    }

    peerB.sin_family      = AF_INET;
    peerB.sin_port        = htons(9003);
    peerB.sin_addr.s_addr = inet_addr("127.0.0.1");

    ccnet_init(1, 16);

    struct ccnet_transport_class *ctc =
        ccnet_tran_class_alloc(C_send, C_recv, C_close, NULL);
    ccnet_trans_class_register(2, ctc);   // 下一跳 B=2

    // 全局拓扑
    ccnet_graph_set_edge(0, 1, 1);
    ccnet_graph_set_edge(1, 2, 1);
    ccnet_recompute_effective_graph();

    printf("[C] 等待 A 的数据...\n");

    uint8_t buf[2048];
    while (1) {
        int n = C_recv(NULL, buf, sizeof(buf));
        if (n > 0) {
            printf("[C] 收到来自 A 的 UDP 包，len=%d\n", n);
            ccnet_input(NULL, buf, n);
        }
    }

    return 0;
}
