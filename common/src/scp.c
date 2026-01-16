#include "scp.h"
#include "hashmap.h"
#include "queue.h"
#include "in_cksum.h"
#include "scp_time.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

static uint32_t scp_clock = 0;
static struct hashmap scp_stream_map;
static struct list_node scp_stream_queue;


static int scp_output(struct scp_stream *ss, int flags);
static struct scp_buf *scp_buf_alloc(uint32_t len)
{
    struct scp_buf *sb = malloc(len);
    *sb = (struct scp_buf) {};
    list_node_init(&sb->node);
    return sb;
}

static void scp_buf_free(struct scp_buf *b)
{
    if (!b) {
        return;
    }
    free(b);
}

struct scp_transport_class *scp_transport_class_alloc(void) 
{
    struct scp_transport_class *ret = malloc(sizeof(struct scp_transport_class));
    if (!ret) {
        return NULL;
    }
    return ret;
}

static void scp_update_rtt(struct scp_stream *s, uint32_t rtt_sample)
{
    if (s->srtt == 0) {
        s->srtt   = rtt_sample;
        s->rttvar = rtt_sample >> 1;
        s->rto    = s->srtt + s->rttvar << 2;
        if (s->rto < SCP_RTO_MIN) s->rto = SCP_RTO_MIN; 
        return;
    }

    int delta = (int)rtt_sample - (int)s->srtt;

    s->srtt   = s->srtt   + (delta >> 3);  // srtt += delta/8
    s->rttvar = s->rttvar + ((abs(delta) - s->rttvar) >> 2); // rttvar += (|delta|-rttvar)/4

    s->rto = s->srtt + s->rttvar << 2;

    if (s->rto < SCP_RTO_MIN) {
        s->rto = SCP_RTO_MIN;
    } 
}

void scp_keeplive(struct scp_stream *ss)
{
    struct scp_hdr *hdr = malloc(sizeof(struct scp_hdr));
    *hdr = (struct scp_hdr){ 
        .seq = ss->snd_nxt, 
        .ack = ss->rcv_nxt, 
        .wnd = htons((uint16_t)ss->rcv_wnd),
        .len = 0, 
        .flags = SCP_FLAG_PING, 
        .fd = ss->src_fd,
    }; 

    ss->st_class->send(ss->st_class->user, hdr, sizeof(struct scp_hdr));

    free(hdr);
}



void scp_timer_process()
{
    scp_clock++;
    struct list_node *ss_node;
    for(ss_node = scp_stream_queue.next; ss_node != &scp_stream_queue; ss_node = ss_node->next) {
        struct scp_stream *ss = container_of(ss_node, struct scp_stream, node);
        for(uint8_t i = 0; i < TIMER_COUNT; i++) {
            if (ss->timer[0] == 0) {
                scp_output(ss, SCP_FLAG_RESEND);
                ss->timeout_count++;
                if (ss->timeout_count > 5) {
                    scp_close(ss->src_fd);
                }
                ss->timer[0] = ss->rto << ss->timeout_count;
            } 
            if (ss->timer[1] == 0) {
                scp_keeplive(ss);
            }
        }
    }

    

}

static void scp_process_data(struct scp_stream *s, struct scp_buf *sb, uint32_t timestamp)
{
    struct scp_hdr *sh = (struct scp_hdr *)sb->data;
    uint32_t seq = ntohl(sh->seq);
    uint32_t payload_len = sb->len - sizeof(struct scp_hdr);
    sb->seq = seq;

    scp_update_rtt(s, scp_clock - timestamp);

    if (SEQ_EQ(seq, s->rcv_nxt)) {
        s->rcv_nxt += payload_len;
        queue_enqueue(&s->rcv_data_q, &sb->node);
        s->sb_cc += payload_len;
        s->rcv_wnd = s->sb_hiwat - s->sb_cc;
        goto try_reassemble;
    }

    if (SEQ_GT(seq, s->rcv_nxt)) {
        struct list_node *p;
        struct scp_buf *b;

        for (p = s->rcv_buf_q.next; p != &s->rcv_buf_q; p = p->next) {
            b = container_of(p, struct scp_buf, node);

            if (SEQ_EQ(seq, b->seq)) {
                scp_buf_free(sb);
                return;
            }

            if (SEQ_LT(seq, b->seq)) {
                list_add_prev(p, &sb->node);
                s->sb_cc += payload_len;
                s->rcv_wnd = s->sb_hiwat - s->sb_cc;
                goto try_reassemble;
            }
        }

        queue_enqueue(&s->rcv_buf_q, &sb->node);
        s->sb_cc += payload_len;
        s->rcv_wnd = s->sb_hiwat - s->sb_cc;
        goto try_reassemble;
    }

    scp_buf_free(sb);
    return;

try_reassemble:
    while (!list_empty(&s->rcv_buf_q)) {
        struct scp_buf *b = container_of(s->rcv_buf_q.next, struct scp_buf, node);
        uint32_t plen = b->len - sizeof(struct scp_hdr);

        if (!SEQ_EQ(b->seq, s->rcv_nxt))
            break;

        list_remove(&b->node);
        queue_enqueue(&s->rcv_data_q, &b->node);
        s->rcv_nxt += plen;
        s->rcv_wnd = s->sb_hiwat - s->sb_cc;

    }

    scp_output(s, SCP_FLAG_ACK);
}

static void scp_process_connect(struct scp_stream *ss, struct scp_buf *sb, uint32_t timestamp)
{
    ss->timestamp = scp_clock;
    ss->state = SCP_ESTABLISHED;
    scp_process_data(ss, sb, timestamp);
}


void scp_snd_buf_free(struct scp_stream *ss, uint32_t ack)
{
    struct list_node *cur = ss->snd_q.next;
    while (cur != &ss->snd_q) {
        struct scp_buf *sb = container_of(cur, struct scp_buf, node);
        struct list_node *next = cur->next;

        if (sb->seq <= ack) {
            list_remove(cur);
            scp_buf_free(sb);
        }

        cur = next;
    }

}

static void scp_process_ack(struct scp_stream *ss, uint32_t ack, uint32_t wnd)
{
    if (SEQ_LT(ack, ss->snd_una) || SEQ_GEQ(ack, ss->snd_nxt)) {
        return;
    }
    ss->snd_una = ack;
    ss->snd_wnd = wnd;
    scp_snd_buf_free(ss, ack);
}

void scp_output_ack(struct scp_stream *ss)
{
   struct scp_hdr *sh = malloc(sizeof(struct scp_hdr));
   if (!sh) return;
    
   sh->ack = ss->rcv_nxt;
   sh->fd = ss->dst_fd;
   sh->flags = SCP_FLAG_ACK;
   sh->len = 0;
   //TODO: update wnd
   sh->wnd = htons(ss->rcv_wnd);
   sh->cksum = in_checksum(sh, sizeof(struct scp_hdr));

   ss->st_class->send(ss->st_class->user, sh, sizeof(struct scp_hdr));
   
   free(sh);
}

void scp_output_data(struct scp_stream *ss, struct scp_buf *sb)
{ 
    uint32_t payload_len = sb->len - sizeof(struct scp_hdr);

    uint32_t flight = ss->snd_nxt - ss->snd_una;
    uint32_t swnd = ss->snd_wnd - flight;

    if (payload_len > swnd) {
        return;
    }

    struct scp_hdr *sh = (struct scp_hdr *)(sb + 1);

    sh->seq   = htonl(sb->seq);
    sh->ack   = htonl(ss->rcv_nxt);
    sh->fd    = htonl(ss->dst_fd);
    sh->flags = SCP_FLAG_DATA;
    sh->len   = htons((uint16_t)(sb->len - sizeof(struct scp_hdr)));
    sh->wnd = htons(ss->rcv_wnd);
    sh->cksum = in_checksum(sh, sb->len);

    ss->st_class->send(ss->st_class->user, sb->data, sb->len);
}

static int scp_output(struct scp_stream *ss, int flags)
{   
    if (flags == SCP_FLAG_ACK) {
        scp_output_ack(ss);     
        return 0;
    }

    struct list_node *sss_node = ss->snd_q.next;
    uint32_t remain_len = 0;
    while(!list_empty(&ss->snd_q)) {
        struct scp_buf *sb_send = container_of(sss_node, struct scp_buf, node);
        if (sb_send->len > MTU) {
            remain_len = sb_send->len - MTU;
            sb_send->len = MTU;
            scp_output_data(ss, sb_send); 

            memmove(sb_send->data, sb_send->data + MTU, remain_len);
        } else {
            scp_output_data(ss, sb_send); 
            list_remove(sss_node);
        }
        
        ss->snd_nxt += (sb_send->len - sizeof(struct scp_hdr));
        sss_node = ss->snd_q.next;      
    }
    
    return 0;
}

int scp_send(int fd, void *buf, size_t len)
{
    struct scp_stream *ss;
    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);

    struct scp_buf *sb = scp_buf_alloc(sizeof(struct scp_buf) + sizeof(struct scp_hdr) + len);
    sb->data = (uint8_t *)sb + sizeof(struct scp_buf); 
    sb->len = sizeof(struct scp_hdr) + len;
    sb->seq = ss->snd_nxt;
    //copy or not copy, this a question.
    uint8_t *pure_data = (uint8_t *)sb->data + sizeof(struct scp_hdr);
    memcpy(pure_data, buf, len);
    queue_enqueue(&ss->snd_q, &sb->node);

    scp_output(ss, SCP_FLAG_DATA);
    return 0;
}

static uint32_t scp_stream_fd = 0;
uint32_t scp_stream_fd_alloc()
{   //lock in future
    scp_stream_fd++;
    return scp_stream_fd;
}

int scp_init(struct scp_transport_class *st_class, size_t max_streams)
{
    struct scp_stream *ss = malloc(sizeof(struct scp_stream));
    if (!ss) {
        return -1;
    }

    *ss = (struct scp_stream) {
        .src_fd  = scp_stream_fd_alloc(),
        .sb_hiwat = 64 * 1024,    
        .rcv_wnd  = 64 * 1024,
        .snd_wnd  = 64 * 1024,   
    };

    ss->st_class = st_class;
    list_node_init(&ss->node);
    list_node_init(&ss->snd_q);
    list_node_init(&ss->rcv_buf_q);
    list_node_init(&ss->rcv_data_q);
    list_node_init(&scp_stream_queue);
    hashmap_init(&scp_stream_map, max_streams, HASHMAP_KEY_INT);
    hashmap_put(&scp_stream_map, (void *)(uintptr_t)(ss->src_fd), ss);
    return ss->src_fd;
}

int scp_input(int fd, void *buf, size_t len)
{
    struct scp_buf *sb;
    struct scp_hdr *sh;
    struct scp_stream *ss;
    uint8_t *payload;
    uint32_t payload_len;

    sb = scp_buf_alloc(sizeof(struct scp_buf) + len);
    if (!sb) {
        return -1;
    }
    memcpy(sb->data, buf, len);
    sb->len = len;

    sh = (struct scp_hdr *)sb->data;
    uint32_t ack = ntohl(sh->ack);
    uint32_t wnd = ntohs(sh->wnd);

    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)sh->fd);

    payload = (uint8_t *)(sh + 1);

    switch (sh->flags) {
        case SCP_FLAG_DATA:
            scp_process_data(ss, sb, scp_clock);
            break;
        case SCP_FLAG_ACK:
            scp_process_ack(ss, ack, wnd);
            break;

        case SCP_FLAG_CONNECT:
            scp_process_connect(ss, sb, scp_clock);
            break;

        default:
            break;
    }
    free(sb);
    return 0;
}

void scp_close(int fd)
{
    struct scp_stream *ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);
    ss->state = SCP_CLOSED;
    hashmap_remove(&scp_stream_map, (void *)(uintptr_t)fd);
    free(ss);
}

int scp_recv(int fd, void *buf, size_t len)
{
    struct scp_stream *s = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);
    if (!s) return -1;

    uint8_t *dst = buf;
    size_t copied = 0;

    while (copied < len && !list_empty(&s->rcv_data_q)) {

        struct list_node *n = s->rcv_data_q.next;
        struct scp_buf *sb = container_of(n, struct scp_buf, node);

        uint32_t payload_len = sb->len - sizeof(struct scp_hdr);

        uint32_t take = (uint32_t)min(len - copied, payload_len);

        uint8_t *payload = sb->data + sizeof(struct scp_hdr);
        memcpy(dst + copied, payload, take);
        copied += take;

        if (take == payload_len) {
            list_remove(n);
            scp_buf_free(sb);
        } else {
            sb->len -= take;
            memmove(sb->data + sizeof(struct scp_hdr),
                    sb->data + sizeof(struct scp_hdr) + take,
                    payload_len - take);
        }
    }

    if (copied > s->sb_cc) {
        s->sb_cc = 0;
    } else {
        s->sb_cc -= copied;
    }

    s->rcv_wnd = s->sb_hiwat - s->sb_cc;

    return copied;
}
