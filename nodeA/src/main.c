#include "scp.h"
#include "cal_udp.h"
#include "scp_time.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

static cal_udp_ctx_t udpA;
static struct sockaddr_in peerB;
static struct scp_transport_class scp_udp_trans;

int scp_udp_send(void *user, const void *buf, size_t len)
{
    return cal_udp_send(&udpA, buf, len, &peerB);
}

int scp_udp_recv(void *user, void *buf, size_t maxlen)
{
    return cal_udp_recv(&udpA, buf, maxlen, &peerB);
}

int scp_udp_close(void *user)
{
    cal_udp_close(&udpA);
    return 0;
}


#define DST_FD 10
#define SRC_FD 20
int main(void)
{
    scp_udp_trans.send  = scp_udp_send;
    scp_udp_trans.recv  = scp_udp_recv;
    scp_udp_trans.close = scp_udp_close;
    scp_udp_trans.user  = NULL;

    scp_init(16);
    scp_stream_alloc(&scp_udp_trans, SRC_FD, DST_FD);

    printf("NodeA: SCP stream opened, fd=%d\n", SRC_FD);

    scp_time_init();

    if (cal_udp_open(&udpA, "0.0.0.0", 9001) < 0) {
        perror("NodeA udp_open");
        return -1;
    }

    int flags = fcntl(udpA.sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
    } else {
        if (fcntl(udpA.sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL O_NONBLOCK");
        } else {
            printf("udp socket set non-blocking\n");
        }
    }

    peerB.sin_family = AF_INET;
    peerB.sin_port   = htons(9002);
    peerB.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t buf[2048];
    uint8_t appbuf[4096];

    char msg[1800] = "hello from NodeA12";
    for(int i = 20; i < 1800; i++) {
        msg[i] = i;
    }
    uint64_t a = 0;
    while (1)
    {
        scp_send(SRC_FD, msg, sizeof(msg));
        
        int n = scp_udp_trans.recv(NULL, (void *)buf, sizeof(buf));
        if (n > 0) {
            scp_input(SRC_FD, buf, n);
        }

        int rn = scp_recv(SRC_FD, appbuf, sizeof(appbuf));
        if (rn > 0) {
            printf("NodeA recv from SCP: %s, a: %lu\n", appbuf, a);
        }
       
        a++;
        usleep(10000);
    }

    return 0;
}
