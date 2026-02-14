#ifndef SCP_H
#define SCP_H

#include <stdint.h>
#include <stddef.h>
#include "link_list.h"


#define MIN_SEG 32
#define SCP_RTO_MIN 50
#define SCP_RECV_LIMIT 0xFFFF
#define SEND_WIN_INIT 0xFFFF
#define RECV_WIN_INIT     0xFFFF
#define MTU 1460


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
    uint32_t sent_off;
    uint8_t *data;
    uint32_t payload_off;
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
#define SCP_FLAG_CONNECT_ACK 0x40
#define SCP_FLAG_FIN         0x80 

#define TIMER_RETRANS 0
#define TIMER_KEEPALIVE 1
#define TIMER_PERSIST 2
#define TIMER_HS      3
#define TIMER_FIN     4
#define TIMER_COUNT   5

enum {
    SCP_CLOSED,
    SCP_SYN_SENT,
    SCP_SYN_RECV,
    SCP_ESTABLISHED,
    SCP_FIN_WAIT,
    SCP_CLOSE_WAIT,
    SCP_LAST_ACK,
};


struct scp_stream {
    struct list_node node;
    struct scp_transport_class *st_class;
    uint32_t timer[TIMER_COUNT];

    uint8_t dst_fd;
    uint8_t src_fd;

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
    uint32_t rtt_seq; //sequence number being timed

    uint8_t  timeout_count;

    struct list_node snd_q;

    struct list_node rcv_buf_q; //none orider
    struct list_node rcv_data_q; // orider
    uint32_t sb_hiwat;
    uint32_t sb_cc;

    uint32_t persist_backoff;  
    uint8_t  zero_wnd;        

    uint32_t timestamp;
    int state;
    uint32_t iss;     // initial send seq
    uint32_t irs;     // initial recv seq
    uint8_t retry;    // handshake/fin retry count
    uint32_t hs_timer;   // handshake timer
    uint32_t fin_timer;  // fin timer
};

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


int scp_init(size_t max_streams);
struct scp_stream *scp_stream_alloc(struct scp_transport_class *st_class, int src_fd, int dst_fd);
int scp_stream_free(struct scp_stream *ss);

int scp_connect(int fd);
int scp_input(void *ctx, void *buf, size_t len);
int scp_send(int fd, void *buf, size_t len);
int scp_recv(int fd, void *buf, size_t len);
void scp_close(int fd);

void scp_timer_process();

#endif
