#include "scp.h"
#include "hashmap.h"
#include "queue.h"
#include "in_cksum.h"
#include "scp_time.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>

static void scp_debug_hex(const char *tag, const void *buf, size_t len)
{
    const uint8_t *p = buf;

    printf("---- %s (%zu bytes) ----\n", tag, len);

    for (size_t i = 0; i < len; i++) {
        printf("%02X ", p[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");

    printf("-----------------------------\n");
}

int a = 0;
static void scp_debug_dump_tx(const char *reason,
                              const void *buf, size_t len)
{
/*
    printf("\n[SCP TX] %s, a:%d\n", reason, a++);
    scp_debug_hex("TX Packet", buf, len); 
*/
}

int b = 0;
static void scp_debug_dump_rx(const void *buf, size_t len)
{
/*
    printf("\n[SCP RX] %d\n", b++);
    scp_debug_hex("RX Packet", buf, len);
*/
}


static pthread_mutex_t scp_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t scp_clock = 0;
static struct hashmap scp_stream_map;
static struct list_node scp_stream_queue;


static int scp_output(struct scp_stream *ss, int flags);
void scp_output_data(struct scp_stream *ss, struct scp_buf *sb,
                     uint32_t offset, uint32_t frag_len);

static struct scp_buf *scp_buf_alloc(uint32_t len)
{
    struct scp_buf *sb = malloc(len);
    if (!sb) return NULL;

    memset(sb, 0, len);
    list_node_init(&sb->node);

    sb->data = (uint8_t *)sb + sizeof(struct scp_buf);

    return sb;
}

static void scp_buf_free(struct scp_buf *b)
{
    if (!b) {
        return;
    }
    free(b);
}

struct scp_stream *scp_stream_alloc(struct scp_transport_class *st_class, int src_fd, int dst_fd)
{
    struct scp_stream *ss = malloc(sizeof(struct scp_stream));
    if (!ss) {
        return NULL;
    }

    *ss = (struct scp_stream) {
        .src_fd   = src_fd,
        .dst_fd   = dst_fd,
        .rto = SCP_RTO_MIN,
        .sb_hiwat = SCP_RECV_LIMIT,    
        .rcv_wnd  = RECV_WIN_INIT,
        .snd_wnd  = SEND_WIN_INIT,  
        .persist_backoff = SCP_RTO_MIN, 
        .zero_wnd = 0, 
    };
    memset(ss->timer, 0, sizeof(ss->timer));

    ss->st_class = st_class;
    list_node_init(&ss->node);
    list_node_init(&ss->snd_q);
    list_node_init(&ss->rcv_buf_q);
    list_node_init(&ss->rcv_data_q);

    queue_enqueue(&scp_stream_queue, &ss->node);
    hashmap_put(&scp_stream_map, (void *)(uintptr_t)(ss->src_fd), ss);

    return ss;
}

int scp_stream_free(struct scp_stream *ss)
{
    if (!ss) {
        return -1;
    }
    list_remove(&ss->node);
    hashmap_remove(&scp_stream_map, (void *)(uintptr_t)(ss->src_fd));
    free(ss);
    return 0;
}

int scp_init(size_t max_streams)
{
    list_node_init(&scp_stream_queue);
    hashmap_init(&scp_stream_map, max_streams, HASHMAP_KEY_INT);
    return 0;
}

static void scp_update_rtt(struct scp_stream *s, uint32_t rtt_sample)
{
    if (s->srtt == 0) {
        s->srtt   = rtt_sample;
        s->rttvar = rtt_sample >> 1;
        s->rto    = s->srtt + (s->rttvar << 2);
        if (s->rto < SCP_RTO_MIN) s->rto = SCP_RTO_MIN; 
        return;
    }

    int delta = (int)rtt_sample - (int)s->srtt;

    s->srtt   = s->srtt   + (delta >> 3);  // srtt += delta/8
    s->rttvar = s->rttvar + ((abs(delta) - s->rttvar) >> 2); // rttvar += (|delta|-rttvar)/4

    s->rto = s->srtt + (s->rttvar << 2);

    if (s->rto < SCP_RTO_MIN) {
        s->rto = SCP_RTO_MIN;
    } 
}

static inline void scp_update_rcv_wnd(struct scp_stream *s)
{
    if (s->sb_cc >= s->sb_hiwat)
        s->rcv_wnd = 0;
    else
        s->rcv_wnd = s->sb_hiwat - s->sb_cc;
}

static void scp_send_window_probe(struct scp_stream *ss)
{
    struct scp_hdr hdr = {
        .seq   = htonl(ss->snd_nxt),  
        .ack   = htonl(ss->rcv_nxt),
        .wnd   = htons((uint16_t)ss->rcv_wnd),
        .len   = 0,
        .cksum = 0,
        .flags = SCP_FLAG_PING,     
        .fd    = ss->dst_fd,
    };

    hdr.cksum = in_checksum(&hdr, sizeof(hdr));
    ss->st_class->send(ss->st_class->user, &hdr, sizeof(hdr));
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

static void scp_retransmit(struct scp_stream *ss)
{
    struct list_node *n;
    //karn
    ss->rtt_ts = 0;

    for (n = ss->snd_q.next; n != &ss->snd_q; n = n->next) {
        struct scp_buf *sb = container_of(n, struct scp_buf, node);

        uint32_t total_payload = sb->len - sizeof(struct scp_hdr);
        uint32_t sent = 0;

        if (SEQ_LEQ(sb->seq + total_payload, ss->snd_una)) {
            continue;
        }

        if (SEQ_LT(sb->seq, ss->snd_una)) {
            sent = ss->snd_una - sb->seq;
            if (sent > total_payload)
                continue;
        }

        while (sent < total_payload) {
            uint32_t frag_len = min((uint32_t)(MTU - sizeof(struct scp_hdr)),
                                    total_payload - sent);

            scp_output_data(ss, sb, sent, frag_len);
            sent += frag_len;
        }
    }
}

void scp_timer_process()
{
    scp_clock++;

    struct list_node *cur, *next;

    pthread_mutex_lock(&scp_lock);

    cur = scp_stream_queue.next;
    while (cur != &scp_stream_queue) {
        next = cur->next;   

        struct scp_stream *ss = container_of(cur, struct scp_stream, node);

        if (ss->timer[TIMER_RETRANS] > 0) {
            ss->timer[TIMER_RETRANS]--;
            if (ss->timer[TIMER_RETRANS] == 0) {
                scp_retransmit(ss);
                ss->timeout_count++;

                if (ss->timeout_count > 6) {
                    ss->state = SCP_CLOSED;
                    cur = next;
                    continue;
                }

                ss->timer[TIMER_RETRANS] = ss->rto << ss->timeout_count;
            }
        }

        if (ss->timer[TIMER_KEEPALIVE] > 0) {
            ss->timer[TIMER_KEEPALIVE]--;
            if (ss->timer[TIMER_KEEPALIVE] == 0) {
                scp_keeplive(ss);
            }
        }

        if (ss->timer[TIMER_PERSIST] > 0) {
            ss->timer[TIMER_PERSIST]--;
            if (ss->timer[TIMER_PERSIST] == 0) {
                scp_send_window_probe(ss);
            }
        }

        cur = next;  
    }

    pthread_mutex_unlock(&scp_lock);
}


static void scp_process_data(struct scp_stream *s, struct scp_buf *sb)
{
    struct scp_hdr *sh = (struct scp_hdr *)sb->data;
    uint32_t seq = ntohl(sh->seq);
    uint32_t payload_len = sb->len - sizeof(struct scp_hdr);
    sb->seq = seq;

    if (SEQ_EQ(seq, s->rcv_nxt)) {
        s->rcv_nxt += payload_len;
        queue_enqueue(&s->rcv_data_q, &sb->node);
        s->sb_cc += payload_len;
        scp_update_rcv_wnd(s);
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
                scp_update_rcv_wnd(s);
                goto try_reassemble;
            }
        }

        queue_enqueue(&s->rcv_buf_q, &sb->node);
        s->sb_cc += payload_len;
        scp_update_rcv_wnd(s);
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
        scp_update_rcv_wnd(s);

    }

    scp_output(s, SCP_FLAG_ACK);
}

static void scp_process_connect(struct scp_stream *ss, struct scp_buf *sb)
{
    ss->timestamp = scp_clock;
    ss->state = SCP_ESTABLISHED;
    scp_process_data(ss, sb);
}


void scp_snd_buf_free(struct scp_stream *ss, uint32_t ack)
{
    struct list_node *cur = ss->snd_q.next;

    while (cur != &ss->snd_q) {
        struct scp_buf *sb   = container_of(cur, struct scp_buf, node);
        struct list_node *next = cur->next;

        uint32_t payload_len = sb->len - sizeof(struct scp_hdr);
        uint32_t end_seq     = sb->seq + payload_len;

        if (SEQ_LEQ(end_seq, ack)) {
            list_remove(cur);
            scp_buf_free(sb);
        }

        cur = next;
    }
}


/*
 * snd_una <= ack <= snd_nxt
 */
static void scp_process_ack(struct scp_stream *ss, uint32_t ack, uint32_t wnd, uint32_t timestamp)
{
    if (SEQ_LT(ack, ss->snd_una) || SEQ_GT(ack, ss->snd_nxt)) {
        return;
    }
    uint32_t old_una = ss->snd_una;

    if (ss->rtt_ts != 0 && SEQ_GEQ(ack, ss->rtt_seq)) { 
        uint32_t sample = scp_clock - ss->rtt_ts; 
        if (sample == 0) sample = 1; // no 0 
        scp_update_rtt(ss, sample); 
        ss->rtt_ts = 0; 
    }

    ss->snd_una = ack;
    ss->snd_wnd = wnd;

    scp_snd_buf_free(ss, ack);

    if (SEQ_GT(ss->snd_una, old_una)) { 
        ss->timeout_count = 0; 
        if (ss->snd_una == ss->snd_nxt) { 
            // No data
            ss->timer[TIMER_RETRANS] = 0; 
        } else { 
            // data is flying
            ss->timer[TIMER_RETRANS] = ss->rto; 
        } 
    }

    // zero wnd and persist backoff
    if (wnd == 0) {
        //start persist
        if (!ss->zero_wnd) {
            ss->zero_wnd = 1;
            ss->timer[TIMER_PERSIST] = ss->persist_backoff;
        }
        if (ss->persist_backoff < 1000) {
            ss->persist_backoff <<= 1;   
        }
    } else {
        //cloase persist
        ss->zero_wnd        = 0;
        ss->persist_backoff = 20;       // recovry persist
        ss->timer[TIMER_PERSIST] = 0;   
    }

}


void scp_output_ack(struct scp_stream *ss)
{
   struct scp_hdr *sh = malloc(sizeof(struct scp_hdr));
   if (!sh) return;
    
   sh->ack = htonl(ss->rcv_nxt);
   sh->fd = ss->dst_fd;
   sh->flags = SCP_FLAG_ACK;
   sh->len = 0;
   sh->wnd = htons(ss->rcv_wnd);
   sh->cksum = in_checksum(sh, sizeof(struct scp_hdr));

   scp_debug_dump_tx("ACK", sh, sizeof(struct scp_hdr));
   ss->st_class->send(ss->st_class->user, sh, sizeof(struct scp_hdr));
   
   free(sh);
}

void scp_output_data(struct scp_stream *ss, struct scp_buf *sb,
                     uint32_t offset, uint32_t frag_len)
{
    struct scp_hdr hdr;

    uint8_t packet[MTU];
    memset(packet, 0, sizeof(packet));

    uint8_t *payload_base = sb->data + sizeof(struct scp_hdr);
    uint8_t *frag_payload = payload_base + offset;

    hdr.seq   = htonl(sb->seq + offset);
    hdr.ack   = htonl(ss->rcv_nxt);
    hdr.wnd   = htons((uint16_t)ss->rcv_wnd);
    hdr.len   = htons((uint16_t)frag_len);
    hdr.cksum = 0;
    hdr.flags = SCP_FLAG_DATA;
    hdr.fd    = ss->dst_fd;

    memcpy(packet, &hdr, sizeof(struct scp_hdr));
    memcpy(packet + sizeof(struct scp_hdr), frag_payload, frag_len);

    hdr.cksum = in_checksum(packet, sizeof(struct scp_hdr) + frag_len);
    memcpy(packet, &hdr, sizeof(struct scp_hdr));

    scp_debug_dump_tx("DATA", packet, sizeof(struct scp_hdr) + frag_len);
    ss->st_class->send(ss->st_class->user,
                       packet,
                       sizeof(struct scp_hdr) + frag_len);
}


static int scp_output(struct scp_stream *ss, int flags)
{
    if (flags == SCP_FLAG_ACK) {
        scp_output_ack(ss);
        return 0;
    }

    if (ss->rtt_ts == 0 && SEQ_EQ(ss->snd_una, ss->snd_nxt)) {
        ss->rtt_ts = scp_clock;
        ss->rtt_seq = ss->snd_nxt;
    }

    struct list_node *n;
    for (n = ss->snd_q.next; n != &ss->snd_q; n = n->next) {
        struct scp_buf *sb_send = container_of(n, struct scp_buf, node);

        uint32_t total_payload = sb_send->len - sizeof(struct scp_hdr);

        uint32_t sent = 0;
        if (SEQ_GT(ss->snd_nxt, sb_send->seq)) {
            sent = ss->snd_nxt - sb_send->seq;
            if (sent >= total_payload) {
                continue;
            }
        }

        while (sent < total_payload) {
            uint32_t frag_len = min((uint32_t)(MTU - sizeof(struct scp_hdr)),
                                    total_payload - sent);

            int64_t flight64 = (int64_t)ss->snd_nxt - (int64_t)ss->snd_una;
            if (flight64 < 0) {
                flight64 = 0;
            } 
            int32_t swnd = (int32_t)ss->snd_wnd - (int32_t)flight64;
            if (swnd < 0) {
                swnd = 0;
                goto out;
            }

            if ((uint32_t)frag_len > (uint32_t)swnd) {
                frag_len = (uint32_t)swnd;
            }

            if (frag_len < MIN_SEG && (total_payload - sent) > frag_len) {
                goto out;
            }

            scp_output_data(ss, sb_send, sent, frag_len);

            uint32_t seg_end = sb_send->seq + sent + frag_len;
            if (SEQ_GT(seg_end, ss->snd_nxt)) {
                ss->snd_nxt = seg_end;
            }

            sent += frag_len;
        }
    }

out:
    if (ss->timer[TIMER_RETRANS] == 0) {
        ss->timer[TIMER_RETRANS] = ss->rto;
        ss->timeout_count = 0;
    }

    return 0;
}



int scp_send(int fd, void *buf, size_t len)
{
    struct scp_stream *ss;

    pthread_mutex_lock(&scp_lock);
    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);

    if (!ss || (ss->state == SCP_CLOSED && ss->snd_nxt != 0)) {
        pthread_mutex_unlock(&scp_lock);
        return -1; 
    }

    uint32_t flight = ss->snd_nxt - ss->snd_una;
    if (flight >= ss->snd_wnd) {
        pthread_mutex_unlock(&scp_lock);
        return -2; // EWOULDBLOCK
    }
    
    struct scp_buf *sb = scp_buf_alloc(sizeof(struct scp_buf) + sizeof(struct scp_hdr) + len);
    sb->data = (uint8_t *)sb + sizeof(struct scp_buf); 
    sb->len = sizeof(struct scp_hdr) + len;
    sb->seq = ss->snd_nxt;
    //copy or not copy, this a question.
    uint8_t *pure_data = (uint8_t *)sb->data + sizeof(struct scp_hdr);
    memcpy(pure_data, buf, len);
    queue_enqueue(&ss->snd_q, &sb->node);
    if (ss->snd_nxt == 0) {
        ss->state = SCP_ESTABLISHED;
        scp_output(ss, SCP_FLAG_CONNECT);
    } else {
        scp_output(ss, SCP_FLAG_DATA);
    }
    pthread_mutex_unlock(&scp_lock);

    return 0;
}

int scp_input(int fd, void *buf, size_t len)
{
    pthread_mutex_lock(&scp_lock); 

    struct scp_buf *sb;
    struct scp_hdr *sh;
    struct scp_stream *ss;

    scp_debug_dump_rx(buf, len);

    sb = scp_buf_alloc(sizeof(struct scp_buf) + len);
    if (!sb) {
        pthread_mutex_unlock(&scp_lock);
        return -1;
    }
    memcpy(sb->data, buf, len);
    sb->len = len;

    sh = (struct scp_hdr *)sb->data;

    uint16_t calc = in_checksum(buf, len); 
    //In internet checksum, one mod algorithm, 0 equl 0xFF.
    if (calc != 0 && calc != 0xFF) { 
        scp_buf_free(sb); 
        pthread_mutex_unlock(&scp_lock);
        return -1; 
    }

    uint32_t ack = ntohl(sh->ack);
    uint32_t wnd = ntohs(sh->wnd);

    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)sh->fd);
    if (!ss) {
        pthread_mutex_unlock(&scp_lock);
        scp_buf_free(sb);
        return -1;
    }

    switch (sh->flags) {
    case SCP_FLAG_DATA:
        scp_process_data(ss, sb);
        break;

    case SCP_FLAG_CONNECT:
        scp_process_connect(ss, sb);
        break;

    case SCP_FLAG_ACK:
        scp_process_ack(ss, ack, wnd, scp_clock);
        scp_buf_free(sb);  
        break;

    case SCP_FLAG_PING: 
        scp_process_ack(ss, ack, wnd, scp_clock); 
        scp_buf_free(sb); 
        break;
    default:
        scp_buf_free(sb);
        break;
    }

    pthread_mutex_unlock(&scp_lock);
    return 0;
}


void scp_close(int fd)
{
    struct scp_stream *ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);

    ss->state = SCP_CLOSED;
    list_remove(&ss->node);
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

    uint32_t old_wnd = s->rcv_wnd;
    scp_update_rcv_wnd(s);

    // fix: persist
    if (old_wnd == 0 && s->rcv_wnd > 0) {
        scp_output(s, SCP_FLAG_ACK);
    }

    return copied;
}
