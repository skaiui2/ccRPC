#include "scp.h"
#include "cal_udp.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "scp_time.h"

static cal_udp_ctx_t udpB;     
static struct sockaddr_in peerA;  
static struct scp_transport_class scp_udp_trans;


// transport: send
int scp_udp_send(void *user, const void *buf, size_t len)
{
    return cal_udp_send(&udpB, buf, len, &peerA);
}

// transport: recv（不使用）
int scp_udp_recv(void *user, void *buf, size_t maxlen)
{
    int n = cal_udp_recv(&udpB, buf, maxlen, &peerA);
    return n;
}

// transport: close
int scp_udp_close(void *user)
{
    cal_udp_close(&udpB);
    return 0;
}

int main(void)
{
    int fd = scp_init(&scp_udp_trans, 16);
    scp_time_init();
    if (cal_udp_open(&udpB, "0.0.0.0", 9002) < 0) {
        perror("NodeB udp_open");
        return -1;
    }

    memset(&peerA, 0, sizeof(peerA));
    peerA.sin_family = AF_INET;
    peerA.sin_port   = htons(9001);
    peerA.sin_addr.s_addr = inet_addr("127.0.0.1");

    scp_udp_trans.send  = scp_udp_send;
    scp_udp_trans.recv  = scp_udp_recv;
    scp_udp_trans.close = scp_udp_close;
    scp_udp_trans.user  = NULL;

    printf("NodeB: SCP stream opened, fd=%d\n", fd);

    const char *msg = "hello from NodeB";
    printf("NodeB send: %s\n", msg);

    uint8_t buf[2048];
    struct sockaddr_in src;
    

    for (;;) 
    {
        int len = strlen(msg) + 1;
        scp_send(fd, (void *)msg, len);
        int n = cal_udp_recv(&udpB, buf, sizeof(buf), &src);
        /*
        if (n > 0) {
            scp_recv(fd, buf, (size_t)n);
        }
        */

        printf("NodeB get: %s\n", buf);

    }

    return 0;
}
