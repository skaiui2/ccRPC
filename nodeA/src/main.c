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
/*
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

    pthread_join(th_call, NULL);

    for (;;)
        pause()
    return 0;
}
*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include "cal_udp.h"
#include "scp.h"

#define SEND_BUF 2048
#define TEST_FILE_SIZE (100 * 1024 * 1024)   

struct scp_udp_user {
    cal_udp_ctx_t *udp;
    struct sockaddr_in peer;
};

static int scp_udp_send(void *user, const void *buf, size_t len)
{
    struct scp_udp_user *u = user;
    return cal_udp_send(u->udp, buf, len, &u->peer);
}

int main()
{
    printf("[A] generating %d bytes test.bin...\n", TEST_FILE_SIZE);

    int gen = open("testA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (size_t i = 0; i < TEST_FILE_SIZE; i++) {
        uint8_t b = i % 256;
        write(gen, &b, 1);
    }
    close(gen);

    int fd_send = open("testA.bin", O_RDONLY);
    int fd_recv = open("outA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    cal_udp_ctx_t udp;
    cal_udp_open(&udp, "127.0.0.1", 5000);

    int fl = fcntl(udp.sockfd, F_GETFL, 0);
    fcntl(udp.sockfd, F_SETFL, fl | O_NONBLOCK);
    srand(time(NULL));

    scp_init(16);

    struct scp_udp_user user;
    user.udp = &udp;
    user.peer.sin_family = AF_INET;
    user.peer.sin_port   = htons(6000);
    user.peer.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct scp_transport_class st = {
        .user  = &user,
        .send  = scp_udp_send,
        .recv  = NULL,
        .close = NULL
    };

    uint8_t sendbuf[SEND_BUF];
    uint8_t recvbuf[2048];
    uint8_t rxbuf[2048];
    struct sockaddr_in src;

    size_t sent = 0;
    size_t received = 0;

    struct scp_stream *ss = scp_stream_alloc(&st, 1, 1);

    scp_connect(1);

    printf("[A] waiting ESTABLISHED...\n");
    while (ss->state != SCP_ESTABLISHED) {
        scp_timer_process();
        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);
        usleep(1000);
    }

    printf("[A] ESTABLISHED, start full-duplex...\n");

    ssize_t cur_len = 0;
    size_t  cur_off = 0;
    int     have_pending = 0;

    while (1) {
        scp_timer_process();

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);

        /* 1. 如果没有挂起数据且还没发完，就读一批 */
        if (!have_pending && sent < TEST_FILE_SIZE) {
            cur_len = read(fd_send, sendbuf, sizeof(sendbuf));
            if (cur_len > 0) {
                cur_off = 0;
                have_pending = 1;
            } else {
                have_pending = 0;
            }
        }

        /* 2. 发送挂起数据（整批语义：要么全排队，要么等） */
        if (have_pending) {
            int ret = scp_send(1, sendbuf + cur_off, (size_t)cur_len - cur_off);
            if (ret == 0) {
                sent += (size_t)cur_len;
                have_pending = 0;
            } else if (ret == -2) {
                /* 窗口满，本轮不读新数据，等下一轮再试 */
            } else {
                goto out;
            }
        }

        /* 3. 接收对端数据 */
        int n = scp_recv(1, recvbuf, sizeof(recvbuf));
        if (n > 0) {
            write(fd_recv, recvbuf, n);
            received += (size_t)n;
        }

        /* 4. 双向都完成：我发完 + 我收完 + 我这边数据都被 ACK */
        if (sent == TEST_FILE_SIZE &&
            ss->snd_una == ss->snd_nxt &&
            received == TEST_FILE_SIZE) {

            printf("[A] full-duplex done, sending FIN...\n");
            scp_close(1);

            int wait_ms = 0;
            while (ss->state != SCP_CLOSED && wait_ms < 5000) {
                scp_timer_process();
                int rn2 = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
                if (rn2 > 0) scp_input(ss, rxbuf, rn2);
                usleep(1000);
                wait_ms++;
            }
            printf("[A] CLOSED or timeout, state=%d\n", ss->state);
            break;
        }

        usleep(1000);
    }

out:
    close(fd_send);
    close(fd_recv);

    printf("ALL down!!!");
    return 0;
}
