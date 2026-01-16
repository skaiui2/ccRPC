#include "rpc.h"
#include "rpc_cal_tcp.h"
#include "services_fs.h"
#include "rpc_gen.h"
#include "scp.h"
#include "cal_udp.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "scp_time.h"

static cal_udp_ctx_t udpA;          
static struct sockaddr_in peerB;    
static int peerB_set = 0;
static struct scp_transport_class scp_udp_trans;


int scp_udp_send(void *user, const void *buf, size_t len)
{
    return cal_udp_send(&udpA, buf, len, &peerB);
}

int scp_udp_recv(void *user, void *buf, size_t maxlen)
{
    int n = cal_udp_recv(&udpA, buf, maxlen, &peerB);
    return n;
}

int scp_udp_close(void *user)
{
    cal_udp_close(&udpA);
    return 0;
}

int main(void)
{
    int fd = scp_init(&scp_udp_trans, 16);
    scp_time_init();
    if (cal_udp_open(&udpA, "0.0.0.0", 9001) < 0) {
        perror("NodeA udp_open");
        return -1;
    }
    
    scp_udp_trans.send  = scp_udp_send;
    scp_udp_trans.recv  = scp_udp_recv;
    scp_udp_trans.close = scp_udp_close;
    scp_udp_trans.user  = NULL;
    
    printf("NodeA: SCP stream opened, fd=%d\n", fd);

    peerB.sin_family = AF_INET;
    peerB.sin_port   = htons(9002);
    peerB.sin_addr.s_addr = inet_addr("127.0.0.1");


    uint8_t buf[2048];
    struct sockaddr_in src;

    const char *msg = "hello from NodeA";
    for (;;) 
    {
        int n = scp_udp_trans.recv(&udpA, buf, sizeof(buf));
        scp_recv(fd, buf, n);
        uint8_t *pd = buf + sizeof(struct scp_hdr);
        printf("NodeA get: %s, len = %d\n", pd, n);
        cal_udp_send(&udpA, msg, strlen(msg) + 1, &peerB);
        //scp_recv(fd, buf, (size_t)n);
    }

    return 0;
}
