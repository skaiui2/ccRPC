#ifndef SCP_H
#define SCP_H

#include <stdint.h>
#include <stddef.h>
#include "link_list.h"

#define SCP_RTO_MIN 20   
#define MTU 1500


struct scp_transport_class {
    int (*send)(void *user, const void *buf, size_t len);
    int (*recv)(void *user, void *buf, size_t maxlen);
    int (*close)(void *user);
    void *user;
};

struct scp_buf {
    struct list_node node;
    size_t len;
    uint32_t seq; // save it from hdr.
    uint8_t *data; 
};

struct scp_hdr {
    uint32_t seq;
    uint32_t ack;
    uint16_t wnd;
    uint16_t len;
    uint16_t cksum;
    uint8_t flags;
    uint8_t fd;
} __attribute__((packed));

#define SCP_FLAG_DATA  0x01
#define SCP_FLAG_ACK   0x02
#define SCP_FLAG_RST   0x04
#define SCP_FLAG_CONNECT  0x08
#define SCP_FLAG_PING  0x10
#define SCP_FLAG_RESEND 0x20

#define TIMER_COUNT 2
struct scp_stream {
    struct list_node node;
    struct scp_transport_class *st_class;
    uint32_t timer[TIMER_COUNT];

    uint32_t dst_fd;
    uint32_t src_fd;

    uint32_t snd_una;
    uint32_t snd_nxt;
    uint32_t snd_wnd;

    uint32_t rcv_nxt;
    uint32_t rcv_wnd;

    uint32_t snd_wmem;
    uint32_t rcv_wmem;

    uint32_t srtt;
    uint32_t rttvar;
    uint32_t rto;
    uint32_t rtt_ts;
    uint8_t  timeout_count;

    struct list_node snd_q;

    struct list_node rcv_buf_q; //none orider 
    struct list_node rcv_data_q; // orider
    uint32_t sb_hiwat;
    uint32_t sb_cc;

    uint32_t timestamp;
    int state;
};

#define SCP_CLOSED       0
#define SCP_ESTABLISHED  1
#define SCP_RESET        2



/*
 * Greater than:
 */
#define SEQ_GT(a,b)  ((int32_t)((a) - (b)) > 0)
#define SEQ_GEQ(a,b) ((int32_t)((a) - (b)) >= 0)

#define SEQ_EQ(a,b) ((int32_t)((a) == (b)))
/*
 * less than:
*/
#define SEQ_LT(a,b)  ((int32_t)((a) - (b)) < 0)
#define SEQ_LEQ(a,b) ((int32_t)((a) - (b)) <= 0)


#define min(a, b) ((a) < (b) ? (a) : (b))
#define imin(a, b) ((int)(a) < (int)(b) ? (int)(a) : (int)(b))


int scp_init(struct scp_transport_class *st_class, size_t max_streams);
int scp_input(int fd, void *buf, size_t len);
int scp_send(int fd, void *buf, size_t len);
int scp_recv(int fd, void *buf, size_t len);
void scp_close(int fd);

void scp_timer_process();

#endif
