#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "ccnet.h"
#include "mq_posix.h"

#define NODE_ID_A 0
#define NODE_ID_C 1
#define NODE_ID_B 2

#define MQ_A_TO_C   "/mq_AtoC"
#define MQ_C_TO_A   "/mq_CtoA"
#define MQ_B_TO_C   "/mq_BtoC"
#define MQ_C_TO_B   "/mq_CtoB"

static int mq_from_A = -1;
static int mq_to_A   = -1;
static int mq_from_B = -1;
static int mq_to_B   = -1;

static void debug_hex(const char *tag, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    printf("---- %s (%zu bytes) ----\n", tag, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", p[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
    printf("-----------------------------\n");
}

int C_send_to_A(void *ctx, void *data, size_t len)
{
    printf("\n[C][TX→A] len=%zu\n", len);
    debug_hex("TX Packet", data, len);
    int ret = mqposix_send(mq_to_A, data, len);
    if (ret < 0) perror("[C] send_to_A");
    return ret;
}

int C_send_to_B(void *ctx, void *data, size_t len)
{
    printf("\n[C][TX→B] len=%zu\n", len);
    debug_hex("TX Packet", data, len);
    int ret = mqposix_send(mq_to_B, data, len);
    if (ret < 0) perror("[C] send_to_B");
    return ret;
}

int C_ccnet_recv(void *user, void *buf, size_t maxlen)
{
    int nA = mqposix_recv(mq_from_A, buf, maxlen);
    if (nA > 0) return nA;

    int nB = mqposix_recv(mq_from_B, buf, maxlen);
    if (nB > 0) return nB;

    return 0;
}

int main(void)
{
    mq_from_A = mqposix_open_receiver(MQ_A_TO_C);
    mq_to_A   = mqposix_open_sender(MQ_C_TO_A);

    mq_from_B = mqposix_open_receiver(MQ_B_TO_C);
    mq_to_B   = mqposix_open_sender(MQ_C_TO_B);

    if (mq_from_A < 0 || mq_to_A < 0 || mq_from_B < 0 || mq_to_B < 0)
        return 1;

    ccnet_init(NODE_ID_C, 16);

    ccnet_register_node_link(NODE_ID_A, C_send_to_A);
    ccnet_register_node_link(NODE_ID_B, C_send_to_B);

    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_C, 1);
    ccnet_graph_set_edge(NODE_ID_C, NODE_ID_A, 1);
    ccnet_graph_set_edge(NODE_ID_C, NODE_ID_B, 1);
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_C, 1);

    ccnet_recompute_effective_graph();

    uint8_t buf[256];

    for (;;) {
        int n = C_ccnet_recv(NULL, buf, sizeof(buf));
        if (n > 0) ccnet_input(NULL, buf, n);
        usleep(1000);
    }

    return 0;
}
