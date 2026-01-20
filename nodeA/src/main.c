#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "rpc.h"
#include "scp.h"
#include "ccnet.h"
#include "rpc_gen.h"
#include "services_fs.h"
#include "mq_posix.h"
#include "scp_time.h"

#define NODE_ID_A 0
#define NODE_ID_C 1
#define NODE_ID_B 2

#define MQ_A_TO_C "/mq_AtoC"
#define MQ_C_TO_A "/mq_CtoA"

static int mq_to_C   = -1;
static int mq_from_C = -1;

static int scp_fd_AtoB = 1;

static struct rpc_transport_class *tAtoB;
static struct rpc_transport_class *tBtoA;

int A_ccnet_send(void *user, void *buf, size_t len)
{
    mqposix_send(mq_to_C, buf, len);
    return (int)len;
}

int A_ccnet_recv(void *user, void *buf, size_t maxlen)
{
    return mqposix_recv(mq_from_C, buf, maxlen);
}

int A_scp_input(void *user, void *buf, size_t len)
{
    scp_input(user, buf, len);
    return 0;
}

int A_ccnet_close(void *user)
{
    return 0;
}

struct scp_ccnet_ctx {
    int peer_node_id;
};

static struct scp_ccnet_ctx scp_ctx_A = {
    .peer_node_id = NODE_ID_B,
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
    /* 建议这里用和 mq_msgsize 一致的缓冲区，比如 8192 */
    uint8_t buf[256];

    for (;;) {
        int n = A_ccnet_recv(NULL, buf, sizeof(buf));
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
    mq_unlink("mq_AtoC");
    mq_unlink("mq_CtoA");
    mq_unlink("mq_BtoC");
    mq_unlink("mq_CtoB");

    printf("[A] 启动\n");

    mq_to_C   = mqposix_open_sender(MQ_A_TO_C);
    mq_from_C = mqposix_open_receiver(MQ_C_TO_A);

    printf("[A] mq_to_C=%d mq_from_C=%d\n", mq_to_C, mq_from_C);
    if (mq_to_C < 0 || mq_from_C < 0) {
        fprintf(stderr, "[A] MQ open failed, abort\n");
        return 1;
    }

    rpc_init();
    rpc_register_all();

    ccnet_init(NODE_ID_A, 16);
    ccnet_register_node_link(NODE_ID_A, A_scp_input);
    ccnet_register_node_link(NODE_ID_C, A_ccnet_send);
    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_C, 1);
    ccnet_graph_set_edge(NODE_ID_C, NODE_ID_B, 1);
    ccnet_recompute_effective_graph();

    scp_init(16);
    struct scp_transport_class scp_trans = {
        .send  = scp_ccnet_send,
        .recv  = scp_ccnet_recv,
        .close = scp_ccnet_close,
        .user  = &scp_ctx_A,
    };
    scp_stream_alloc(&scp_trans, scp_fd_AtoB, scp_fd_AtoB);

    tBtoA = rpc_trans_class_alloc(rpc_scp_send, rpc_scp_recv, rpc_scp_close, &scp_fd_AtoB);
    rpc_register_transport(tBtoA);
    rpc_transport_register("fs.read", tBtoA);

    tAtoB = rpc_trans_class_alloc(rpc_scp_send, rpc_scp_recv, rpc_scp_close, &scp_fd_AtoB);
    rpc_register_transport(tAtoB);
    rpc_transport_register("shell.exec", tAtoB);
    scp_time_init();

    pthread_t th_poll, th_call;
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_create(&th_call, NULL, call_thread, NULL);

    /* 原来这里只等 call_thread，call_thread 结束后进程就退出了。
       改成也等待 poll_thread，或者直接阻塞主线程。 */

    pthread_join(th_call, NULL);

    /* 如果你希望节点一直活着，就不要退出主线程： */
    for (;;)
        pause();

    /* 如果你以后想优雅退出，再加退出条件和 pthread_cancel/th_poll 等。 */
    return 0;
}
