#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ccnet.h"
#include "cal_udp.h"

static cal_udp_ctx_t udpA;
static struct sockaddr_in peerC;   // A -> C

int A_send(void *user, void *buf, size_t len)
{
    return cal_udp_send(&udpA, buf, len, &peerC);
}

int A_recv(void *user, void *buf, size_t maxlen)
{
    return 0; // A 不接收
}

int A_close(void *user)
{
    cal_udp_close(&udpA);
    return 0;
}

int main(void)
{
    printf("[A] 启动\n");

    if (cal_udp_open(&udpA, "0.0.0.0", 9001) < 0) {
        perror("udp_open");
        return -1;
    }

    // C 的地址
    peerC.sin_family      = AF_INET;
    peerC.sin_port        = htons(9002);
    peerC.sin_addr.s_addr = inet_addr("127.0.0.1");

    ccnet_init(0, 16);

    struct ccnet_transport_class *ctc =
        ccnet_tran_class_alloc(A_send, A_recv, A_close, NULL);
    ccnet_trans_class_register(1, ctc);   // 下一跳 C=1

    // 全局拓扑
    ccnet_graph_set_edge(0, 1, 1);
    ccnet_graph_set_edge(1, 2, 1);
    ccnet_recompute_effective_graph();

    char msg[] = "hello B";
    struct ccnet_send_parameter csp = {
        .dst  = 2,
        .ttl  = CCNET_TTL,
        .type = CCNET_TYPE_DATA,
    };

    printf("[A] 发送 DATA 给 B\n");
    ccnet_output(&csp, msg, strlen(msg));

    return 0;
}
