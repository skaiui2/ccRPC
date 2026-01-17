#include "scp.h"
#include "cal_udp.h"
#include "scp_time.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static cal_udp_ctx_t udpB;
static struct sockaddr_in peerA;
static struct scp_transport_class scp_udp_trans;

int scp_udp_send(void *user, const void *buf, size_t len)
{
    return cal_udp_send(&udpB, buf, len, &peerA);
}

int scp_udp_recv(void *user, void *buf, size_t maxlen)
{
    return cal_udp_recv(&udpB, buf, maxlen, &peerA);
}

int scp_udp_close(void *user)
{
    cal_udp_close(&udpB);
    return 0;
}

#define DST_FD 20
#define SRC_FD 10

int main(void)
{
    scp_udp_trans.send  = scp_udp_send;
    scp_udp_trans.recv  = scp_udp_recv;
    scp_udp_trans.close = scp_udp_close;
    scp_udp_trans.user  = NULL;

    
    scp_init(16);
    scp_stream_alloc(&scp_udp_trans, SRC_FD, DST_FD);

    printf("NodeB: SCP stream opened, fd=%d\n", SRC_FD);

    scp_time_init();

    if (cal_udp_open(&udpB, "0.0.0.0", 9002) < 0) {
        perror("NodeB udp_open");
        return -1;
    }


    int flags = fcntl(udpB.sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
    } else {
        if (fcntl(udpB.sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL O_NONBLOCK");
        } else {
            printf("udp socket set non-blocking\n");
        }
    }

    peerA.sin_family = AF_INET;
    peerA.sin_port   = htons(9001);
    peerA.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t buf[2048];
    uint8_t appbuf[4096];

    char msg[1800] = "hello from NodeBOP";
    for(int i = 20; i < 1800; i++) {
        msg[i] = i;
    }
    uint64_t a = 0;
    uint64_t count = 0;
    while (1)
    {
        int n = scp_udp_trans.recv(NULL, (void *)buf, sizeof(buf));
        if (n > 0) {
            scp_input(SRC_FD, buf, n);
        }

        int rn = scp_recv(SRC_FD, appbuf, sizeof(appbuf));
        if (rn > 0) {
            count += rn;
            printf("scp recv count %lu\n", count);
            printf("NodeB recv from SCP: %s, a: %lu\n", appbuf, a++);
        }

        scp_send(SRC_FD, msg, strlen(msg) + 1);

        usleep(100);
    }

    return 0;
}
