#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "rpc.h"
#include "rpc_gen.h"
#include "scp.h"
#include "ccnet.h"
#include "mq_posix.h"
#include "scp_time.h"
/*
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
*/
/*
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "cal_udp.h"
#include "scp.h"

#define RECV_BUF 2048
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
    printf("[B] full-duplex test, expecting %d bytes...\n", TEST_FILE_SIZE);

    int gen = open("testB.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (size_t i = 0; i < TEST_FILE_SIZE; i++) {
        uint8_t b = (i * 7) % 256;
        write(gen, &b, 1);
    }
    close(gen);

    int fd_send = open("testB.bin", O_RDONLY);
    int fd_recv = open("outB.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    cal_udp_ctx_t udp;
    cal_udp_open(&udp, "127.0.0.1", 6000);

    int fl = fcntl(udp.sockfd, F_GETFL, 0);
    fcntl(udp.sockfd, F_SETFL, fl | O_NONBLOCK);
    srand(time(NULL));

    scp_init(16);
    scp_time_init();

    struct scp_udp_user user;
    user.udp = &udp;
    user.peer.sin_family = AF_INET;
    user.peer.sin_port   = htons(5000);
    user.peer.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct scp_transport_class st = {
        .user  = &user,
        .send  = scp_udp_send,
        .recv  = NULL,
        .close = NULL
    };

    uint8_t sendbuf[RECV_BUF];
    uint8_t recvbuf[RECV_BUF];
    uint8_t rxbuf[RECV_BUF];
    struct sockaddr_in src;

    size_t sent = 0;
    size_t received = 0;

    struct scp_stream *ss = scp_stream_alloc(&st, 1, 1);

    printf("[B] waiting ESTABLISHED...\n");
    while (ss->state != SCP_ESTABLISHED) {
        scp_timer_process();
        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);
        usleep(1000);
    }

    printf("[B] ESTABLISHED, start full-duplex...\n");

    ssize_t cur_len = 0;
    size_t  cur_off = 0;
    int     have_pending = 0;

    while (1) {
        scp_timer_process();

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);

        if (!have_pending && sent < TEST_FILE_SIZE) {
            cur_len = read(fd_send, sendbuf, sizeof(sendbuf));
            if (cur_len > 0) {
                cur_off = 0;
                have_pending = 1;
            } else {
                have_pending = 0;
            }
        }

        if (have_pending) {
            int ret = scp_send(1, sendbuf + cur_off, (size_t)cur_len - cur_off);
            if (ret == 0) {
                sent += (size_t)cur_len;
                have_pending = 0;
            } else if (ret == -2) {

            } else {
                goto out;
            }
        }

        int n = scp_recv(1, recvbuf, sizeof(recvbuf));
        if (n > 0) {
            write(fd_recv, recvbuf, n);
            received += (size_t)n;
        }

        if (sent == TEST_FILE_SIZE &&
            ss->snd_una == ss->snd_nxt &&
            received == TEST_FILE_SIZE) {

            printf("[B] full-duplex done, sending FIN...\n");
            scp_close(1);

            while (ss->state != SCP_CLOSED) {
                scp_timer_process();
                int rn2 = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
                if (rn2 > 0) scp_input(ss, rxbuf, rn2);
                usleep(1000);
            }
            printf("[B] CLOSED.\n");
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
*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "cal_udp.h"
#include "ikcp.h"
#include "scp_time.h"

#define RECV_BUF 2048
#define TEST_FILE_SIZE (100 * 1024 * 1024)

struct kcp_udp_user {
    cal_udp_ctx_t *udp;
    struct sockaddr_in peer;
};

static int kcp_udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    struct kcp_udp_user *u = user;
    return cal_udp_send(u->udp, buf, len, &u->peer);
}

int main()
{
    scp_time_init();
    int gen = open("testB.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (size_t i = 0; i < TEST_FILE_SIZE; i++) {
        uint8_t b = (i * 7) % 256;
        write(gen, &b, 1);
    }
    close(gen);

    int fd_send = open("testB.bin", O_RDONLY);
    int fd_recv = open("outB.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    cal_udp_ctx_t udp;
    cal_udp_open(&udp, "127.0.0.1", 6000);

    int fl = fcntl(udp.sockfd, F_GETFL, 0);
    fcntl(udp.sockfd, F_SETFL, fl | O_NONBLOCK);

    struct kcp_udp_user user;
    user.udp = &udp;
    user.peer.sin_family = AF_INET;
    user.peer.sin_port   = htons(5000);
    user.peer.sin_addr.s_addr = inet_addr("127.0.0.1");

    ikcpcb *kcp = ikcp_create(1, &user);
    ikcp_setoutput(kcp, kcp_udp_output);
    ikcp_wndsize(kcp, 1024, 1024);
    ikcp_nodelay(kcp, 1, 10, 2, 1);

    uint8_t sendbuf[RECV_BUF];
    uint8_t recvbuf[RECV_BUF];
    uint8_t rxbuf[RECV_BUF];
    struct sockaddr_in src;

    size_t sent = 0;
    size_t received = 0;

    ssize_t cur_len = 0;
    size_t  cur_off = 0;
    int     have_pending = 0;

    while (1) {
        ikcp_update(kcp, (IUINT32)scp_now_time());

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) ikcp_input(kcp, (const char *)rxbuf, rn);

        if (!have_pending && sent < TEST_FILE_SIZE) {
            cur_len = read(fd_send, sendbuf, sizeof(sendbuf));
            if (cur_len > 0) {
                cur_off = 0;
                have_pending = 1;
            } else {
                have_pending = 0;
            }
        }

        if (have_pending) {
            int ret = ikcp_send(kcp, (const char *)(sendbuf + cur_off),
                                (int)(cur_len - (ssize_t)cur_off));
            if (ret >= 0) {
                sent += (size_t)cur_len;
                have_pending = 0;
            } else {
                goto out;
            }
        }

        while (1) {
            int n = ikcp_recv(kcp, (char *)recvbuf, sizeof(recvbuf));
            if (n <= 0) break;
            write(fd_recv, recvbuf, n);
            received += (size_t)n;
        }

       if (sent == TEST_FILE_SIZE &&
            received == TEST_FILE_SIZE &&
            ikcp_waitsnd(kcp) == 0) {

            printf("KCP time = %u ms\n", scp_now_time());
            printf("KCP data_bytes      = %llu\n", (unsigned long long)kcp->stat_data_bytes);
            printf("KCP total_bytes     = %llu\n", (unsigned long long)kcp->stat_total_bytes);
            printf("KCP retrans_bytes   = %llu\n", (unsigned long long)kcp->stat_retrans_bytes);
            if (kcp->stat_total_bytes > 0) {
                double eff = (double)kcp->stat_data_bytes / (double)kcp->stat_total_bytes;
                printf("KCP efficiency      = %.3f\n", eff);
            }
            break;
        }

        usleep(1000);
    }

out:
    ikcp_release(kcp);
    close(fd_send);
    close(fd_recv);
    return 0;
}
