#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "rpc.h"
#include "rpc_gen.h"
#include "scp.h"
#include "ccnet.h"
#include "mq_posix.h"
#include "scp_time.h"

#define NODE_ID_A 0
#define NODE_ID_C 1
#define NODE_ID_B 2

#define MQ_C_TO_B "/mq_CtoB"
#define MQ_B_TO_C "/mq_BtoC"

static int mq_from_C = -1;
static int mq_to_C   = -1;

static int scp_fd_B = 1;

static struct rpc_transport_class *tAtoB;
static struct rpc_transport_class *tBtoA;

int B_ccnet_send(void *user, void *buf, size_t len)
{
    int next_hop = (int)(long)user;

    printf("\n[B][ccnet TX] next_hop=%d len=%zu\n", next_hop, len);
    int r = mqposix_send(mq_to_C, buf, len);
    if (r < 0) {
        perror("[B] mqposix_send FAILED");
    } else {
        printf("[B] mqposix_send OK, len=%d\n", r);
    }

    return r;
}

int B_ccnet_recv(void *user, void *buf, size_t maxlen)
{
    return mqposix_recv(mq_from_C, buf, maxlen);
}

int B_ccnet_close(void *user)
{
    return 0;
}


int B_scp_input(void *user, void *buf, size_t len)
{
    scp_input(user, buf, len);
    return 0;
}

struct scp_ccnet_ctx {
    int peer_node_id;
};

static struct scp_ccnet_ctx scp_ctx_B = {
    .peer_node_id = NODE_ID_A,
};

int scp_ccnet_send(void *user, const void *buf, size_t len)
{
    struct scp_ccnet_ctx *ctx = (struct scp_ccnet_ctx *)user;
    struct ccnet_send_parameter csp = {
        .dst  = ctx->peer_node_id,
        .ttl  = CCNET_TTL,
        .type = CCNET_TYPE_DATA,
    };
    return ccnet_output(&csp, (void *)buf, (int)len);
}

int scp_ccnet_recv(void *user, void *buf, size_t maxlen)
{
    printf("scp_ccnet_recv\r\n");
    return -1;
}

int scp_ccnet_close(void *user)
{
    return 0;
}

ssize_t rpc_scp_send(void *user, const uint8_t *buf, size_t len)
{
    int fd = *(int *)user;
    return scp_send(fd, (void *)buf, len);
}

ssize_t rpc_scp_recv(void *user, uint8_t *buf, size_t maxlen)
{
    int fd = *(int *)user;
    return scp_recv(fd, buf, maxlen);
}

void rpc_scp_close(void *user)
{
    int fd = *(int *)user;
    scp_close(fd);
}

static void *poll_thread(void *arg)
{
    uint8_t buf[256];

    for (;;) {
        int n = B_ccnet_recv(NULL, buf, sizeof(buf));
        if (n > 0)
            ccnet_input(NULL, buf, n);

        rpc_poll();
        usleep(1000);
    }
    return NULL;
}

static void *call_thread(void *arg)
{
    sleep(1);

    struct rpc_param_fs_read p = {
        .path   = "/etc/hostname",
        .offset = 0,
        .size   = 128,
    };
    struct rpc_result_fs_read r;

    int st = rpc_call_fs_read(&p, &r);
    printf("NodeB: fs.read => %d\n", st);
    if (st == 0)
        printf("NodeB: len=%u eof=%u\n", r.len, r.eof);

    return NULL;
}

int main(void)
{
    printf("[B] 启动\n");
    printf("[B] MQ_B_TO_C = '%s'\n", MQ_B_TO_C);
printf("[C] MQ_B_TO_C = '%s'\n", MQ_B_TO_C);


    mq_from_C = mqposix_open_receiver(MQ_C_TO_B);
    mq_to_C   = mqposix_open_sender(MQ_B_TO_C);

    rpc_init();
    rpc_register_all();

    ccnet_init(NODE_ID_B, 16);

    ccnet_register_node_link(NODE_ID_B, scp_input);
    ccnet_register_node_link(NODE_ID_C, B_ccnet_send);

    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_C, 1);
    ccnet_graph_set_edge(NODE_ID_C, NODE_ID_A, 1);
    ccnet_recompute_effective_graph();

    scp_init(16);
    struct scp_transport_class scp_trans = {
        .send  = scp_ccnet_send,
        .recv  = scp_ccnet_recv,
        .close = scp_ccnet_close,
        .user  = &scp_ctx_B,
    };
    scp_stream_alloc(&scp_trans, scp_fd_B, scp_fd_B);

    tBtoA = rpc_trans_class_alloc(rpc_scp_send, rpc_scp_recv, rpc_scp_close, &scp_fd_B);
    rpc_register_transport(tBtoA);
    rpc_transport_register("fs.read", tBtoA);

    tAtoB = rpc_trans_class_alloc(rpc_scp_send, rpc_scp_recv, rpc_scp_close, &scp_fd_B);
    rpc_register_transport(tAtoB);
    rpc_transport_register("shell.exec", tAtoB);
    scp_time_init();

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    pthread_join(th_call, NULL);

    for (;;)
        pause();

    return 0;
}
