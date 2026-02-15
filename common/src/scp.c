#include "scp.h"
#include "hashmap.h"
#include "queue.h"
#include "in_cksum.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

#define SCP_DEBUG
static void scp_debug_hex(const char *tag, const void *buf, size_t len)
{
#ifndef SCP_DEBUG
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
#endif
}

static int a = 0;
static void scp_debug_dump_tx(const char *reason,
                              const void *buf, size_t len)
{
#ifndef SCP_DEBUG
    printf("\n[SCP TX] %s, a:%d\n", reason, a++);
    scp_debug_hex("TX Packet", buf, len);
#endif
}

static int b = 0;
static void scp_debug_dump_rx(const void *buf, size_t len)
{
#ifndef SCP_DEBUG
    printf("\n[SCP RX] %d\n", b++);
    scp_debug_hex("RX Packet", buf, len);
#endif
}


static uint32_t scp_clock = 0;
static struct hashmap scp_stream_map;
static struct list_node scp_stream_queue;

static void scp_dump_hdr(struct scp_stream *ss,
                         const char *dir,
                         const struct scp_hdr *h)
{
    uint32_t seq = ntohl(h->seq);
    uint32_t ack = ntohl(h->ack);
    uint32_t wnd = ntohs(h->wnd);
    uint32_t len = ntohs(h->len);

    int sndq = 0, rcvq = 0;
    struct list_node *n;

    for (n = ss->snd_q.next; n != &ss->snd_q; n = n->next) sndq++;
    for (n = ss->rcv_buf_q.next; n != &ss->rcv_buf_q; n = n->next) rcvq++;

    printf("{\"t\":%u,"
           "\"dir\":\"%s\","
           "\"seq\":%u,"
           "\"ack\":%u,"
           "\"len\":%u,"
           "\"wnd\":%u,"
           "\"flags\":%u,"
           "\"snd_una\":%u,"
           "\"snd_nxt\":%u,"
           "\"rcv_nxt\":%u,"
           "\"snd_wnd\":%u,"
           "\"rcv_wnd\":%u,"
           "\"snd_q\":%d,"
           "\"rcv_q\":%d}\n",
           scp_clock,
           dir,
           seq,
           ack,
           len,
           wnd,
           h->flags,
           ss->snd_una,
           ss->snd_nxt,
           ss->rcv_nxt,
           ss->snd_wnd,
           ss->rcv_wnd,
           sndq,
           rcvq);
}

static uint32_t random32(void)
{
    uint32_t r = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    return r ? r : 1;  
}

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
            .rcv_nxt = 0,
            .snd_nxt = 0,
            .snd_una = 0,
    };
    memset(ss->timer, 0, sizeof(ss->timer));

    ss->st_class = st_class;
    ss->state = SCP_CLOSED;
    ss->iss = 0;
    ss->snd_nxt = 0;
    ss->snd_una = 0;

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
    uint32_t old = s->rcv_wnd;

    if (s->sb_cc >= s->sb_hiwat)
        s->rcv_wnd = 0;
    else
        s->rcv_wnd = s->sb_hiwat - s->sb_cc;

    if (old != 0 && s->rcv_wnd == 0) {
        struct list_node *p;
        /*
        printf("### RCV_WND->0: rcv_nxt=%u, sb_cc=%u, hiwat=%u, rcv_buf_q:\n",
               s->rcv_nxt, s->sb_cc, s->sb_hiwat);
        */
        for (p = s->rcv_buf_q.next; p != &s->rcv_buf_q; p = p->next) {
            struct scp_buf *b = container_of(p, struct scp_buf, node);
            uint32_t plen = b->len - sizeof(struct scp_hdr);
            /*
            printf("    [buf] seq=%u len=%u end=%u\n",
                   b->seq, plen, b->seq + plen);
            */
        }
    }
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
    scp_dump_hdr(ss, "WINDOWS_PING", &hdr);
    ss->st_class->send(ss->st_class->user, &hdr, sizeof(hdr));
}

void scp_keeplive(struct scp_stream *ss)
{
    struct scp_hdr *hdr = malloc(sizeof(struct scp_hdr));
    *hdr = (struct scp_hdr){
            .seq = htonl(ss->snd_nxt),
            .ack = htonl(ss->rcv_nxt),
            .wnd = htons((uint16_t)ss->rcv_wnd),
            .len = 0,
            .flags = SCP_FLAG_PING,
            .fd = ss->dst_fd,
    };
    hdr->cksum = in_checksum(hdr, sizeof(struct scp_hdr));
    scp_dump_hdr(ss, "KEEPLIVE_PING", hdr);
    ss->st_class->send(ss->st_class->user, hdr, sizeof(struct scp_hdr));

    free(hdr);
}

static void scp_retransmit(struct scp_stream *ss)
{
    struct list_node *n;

    /* Karn：重传时不再做 RTT 采样 */
    ss->rtt_ts = 0;
/*
    printf("### RETRANS_START: snd_una=%u snd_nxt=%u\n",
           ss->snd_una, ss->snd_nxt);
*/
    for (n = ss->snd_q.next; n != &ss->snd_q; n = n->next) {
        struct scp_buf *sb = container_of(n, struct scp_buf, node);

        uint32_t total_payload = sb->len - sizeof(struct scp_hdr);
        if (total_payload == 0)
            continue;

        uint32_t start_off = 0;
        uint32_t seg_start = sb->seq;
        uint32_t seg_end   = sb->seq + total_payload;

        /* 已完全确认的段跳过 */
        if (SEQ_LEQ(seg_end, ss->snd_una)) {
            /*
            printf("### RETRANS_SKIP_FULLY_ACKED: seg=[%u,%u) snd_una=%u\n",
                   seg_start, seg_end, ss->snd_una);
            */
            continue;
        }

        /* 部分确认：从 snd_una 之后开始重传 */
        if (SEQ_LT(seg_start, ss->snd_una)) {
            uint32_t trim = ss->snd_una - seg_start;
            if (trim >= total_payload) {
                /*
                printf("### RETRANS_SKIP_TRIM_FULL: seg=[%u,%u) snd_una=%u\n",
                       seg_start, seg_end, ss->snd_una);
                */
                continue;
            }
            start_off = trim;
        }
/*
        printf("### RETRANS_SEG: orig=[%u,%u) retrans_from=%u\n",
               seg_start, seg_end, sb->seq + start_off);
*/
        while (start_off < total_payload) {
            uint32_t remain   = total_payload - start_off;
            uint32_t frag_len = min((uint32_t)(MTU - sizeof(struct scp_hdr)), remain);

            if (frag_len == 0)
                break;

            uint32_t frag_seq = sb->seq + start_off;
            //printf("### RETRANS_FRAG: seq=%u len=%u\n", frag_seq, frag_len);

            scp_output_data(ss, sb, start_off, frag_len);
            start_off += frag_len;
        }
    }

    //printf("### RETRANS_END\n");
}

static void scp_output_connect(struct scp_stream *ss)
{
    struct scp_hdr hdr = {
        .seq = htonl(ss->iss),
        .ack = 0,
        .wnd = htons((uint16_t)ss->rcv_wnd),
        .len = 0,
        .flags = SCP_FLAG_CONNECT,
        .fd = ss->dst_fd,
    };
    hdr.cksum = in_checksum(&hdr, sizeof(hdr));
    scp_debug_dump_tx("CONNECT", &hdr, sizeof(hdr));
    scp_dump_hdr(ss, "CONNECT", &hdr);
    ss->st_class->send(ss->st_class->user, &hdr, sizeof(hdr));
}

static void scp_output_connect_ack(struct scp_stream *ss)
{
    struct scp_hdr hdr = {
        .seq = htonl(ss->snd_nxt),
        .ack = htonl(ss->rcv_nxt),
        .wnd = htons((uint16_t)ss->rcv_wnd),
        .len = 0,
        .flags = SCP_FLAG_CONNECT_ACK,
        .fd = ss->dst_fd,
    };
    hdr.cksum = in_checksum(&hdr, sizeof(hdr));
    scp_debug_dump_tx("CONNECT_ACK", &hdr, sizeof(hdr));
    scp_dump_hdr(ss, "CONNECT_ACK", &hdr);
    ss->st_class->send(ss->st_class->user, &hdr, sizeof(hdr));
}

static void scp_output_fin(struct scp_stream *ss)
{
    struct scp_hdr hdr;

    memset(&hdr, 0, sizeof(hdr));
    hdr.seq  = htonl(ss->snd_nxt);    
    hdr.ack  = htonl(ss->rcv_nxt);
    hdr.wnd  = htons((uint16_t)ss->rcv_wnd);
    hdr.len  = 0;
    hdr.flags = SCP_FLAG_FIN | SCP_FLAG_ACK;  
    hdr.fd   = ss->dst_fd;

    hdr.cksum = in_checksum(&hdr, sizeof(hdr));

    scp_debug_dump_tx("FIN", &hdr, sizeof(hdr));
    scp_dump_hdr(ss, "FIN", &hdr);
    ss->st_class->send(ss->st_class->user, &hdr, sizeof(hdr));
}

static void scp_handle_handshake_timeout(struct scp_stream *ss)
{
    if (ss->retry++ < 5) {
        if (ss->state == SCP_SYN_SENT)
            scp_output_connect(ss);
        else if (ss->state == SCP_SYN_RECV)
            scp_output_connect_ack(ss);

        ss->hs_timer = ss->rto << ss->retry;
        return;
    }

    ss->state = SCP_CLOSED;
}

static void scp_handle_fin_timeout(struct scp_stream *ss)
{
    if (ss->state != SCP_FIN_WAIT && ss->state != SCP_LAST_ACK) {
        ss->fin_timer = 0;
        return;
    }

    if (ss->retry++ < 5) {
        scp_output_fin(ss);
        ss->fin_timer = ss->rto << ss->retry;
        return;
    }

    ss->state = SCP_CLOSED;
    scp_stream_free(ss);
}

void scp_timer_process()
{
    scp_clock++;

    struct list_node *cur, *next;

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
                    scp_stream_free(ss);
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

        if (ss->hs_timer > 0) { 
            ss->hs_timer--; 
            if (ss->hs_timer == 0) { 
                scp_handle_handshake_timeout(ss); 
            } 
        } 
        
        if (ss->fin_timer > 0) {
            ss->fin_timer--; 
            if (ss->fin_timer == 0) { 
                scp_handle_fin_timeout(ss); 
            } 
        }

        cur = next;
    }

}

static void scp_process_data(struct scp_stream *s, struct scp_buf *sb)
{
    struct scp_hdr *sh = (struct scp_hdr *)sb->data;
    uint32_t seq         = ntohl(sh->seq);
    uint32_t payload_len = ntohs(sh->len);

    if (payload_len == 0) {
        scp_buf_free(sb);
        return;
    }

    sb->seq = seq;
    uint32_t end = seq + payload_len;

    /* 0. 清理 rcv_buf_q 中所有已经完全落在 rcv_nxt 之前的段 */
    {
        struct list_node *p = s->rcv_buf_q.next;
        while (p != &s->rcv_buf_q) {
            struct scp_buf *b = container_of(p, struct scp_buf, node);
            struct list_node *next = p->next;

            uint32_t b_end = b->seq + (b->len - sizeof(struct scp_hdr));
            if (SEQ_LEQ(b_end, s->rcv_nxt)) {
                uint32_t plen = b->len - sizeof(struct scp_hdr);
                s->sb_cc -= plen;
                list_remove(p);
                scp_buf_free(b);
            }

            p = next;
        }
    }

    /* 1. 完全在 rcv_nxt 之前的老数据，直接丢弃 */
    if (SEQ_LEQ(end, s->rcv_nxt)) {
        scp_buf_free(sb);
        return;
    }

    /* 2. 与 rcv_nxt 有重叠：裁掉前面已经收到的部分 */
    if (SEQ_LT(seq, s->rcv_nxt)) {
        uint32_t trim = s->rcv_nxt - seq;
        seq         += trim;
        payload_len -= trim;

        uint8_t *payload = sb->data + sizeof(struct scp_hdr);
        memmove(payload, payload + trim, payload_len);

        sh->seq = htonl(seq);
        sh->len = htons((uint16_t)payload_len);

        sb->seq = seq;
        sb->len = sizeof(struct scp_hdr) + payload_len;

        end = seq + payload_len;
    }

    /* 这里保证：seq >= rcv_nxt 且 payload_len > 0 */

    /* 3. 刚好是下一个期望字节：直接按序交付 */
    if (SEQ_EQ(seq, s->rcv_nxt)) {
        /* 3.1 交付当前段 */
        s->rcv_nxt += payload_len;
        queue_enqueue(&s->rcv_data_q, &sb->node);
        s->sb_cc += payload_len;
        scp_update_rcv_wnd(s);

        /* 3.2 再从 rcv_buf_q 里拼接连续段 */
        while (!list_empty(&s->rcv_buf_q)) {
            struct scp_buf *b = container_of(s->rcv_buf_q.next,
                                             struct scp_buf, node);
            uint32_t b_seq  = b->seq;
            uint32_t b_plen = b->len - sizeof(struct scp_hdr);

            if (!SEQ_EQ(b_seq, s->rcv_nxt))
                break;

            list_remove(&b->node);
            queue_enqueue(&s->rcv_data_q, &b->node);
            s->rcv_nxt += b_plen;
            /* sb_cc 在插入 rcv_buf_q 时已经加过，这里只是队列迁移，不再重复加 */
            scp_update_rcv_wnd(s);
        }

        scp_output(s, SCP_FLAG_ACK);
        return;
    }

    /* 4. seq > rcv_nxt：乱序数据，插入 rcv_buf_q，保证不重叠 */

    struct list_node *pos;
    struct scp_buf *b;

    /* 找到第一个 b->seq > seq 的位置，按 seq 升序插入 */
    for (pos = s->rcv_buf_q.next; pos != &s->rcv_buf_q; pos = pos->next) {
        b = container_of(pos, struct scp_buf, node);
        if (SEQ_GT(b->seq, seq))
            break;
    }

    /* 4.1 左侧重叠：看前一个节点 prev 是否覆盖了新段的头部 */
    if (pos->prev != &s->rcv_buf_q) {
        struct scp_buf *prev = container_of(pos->prev, struct scp_buf, node);
        uint32_t p_seq = prev->seq;
        uint32_t p_end = p_seq + (prev->len - sizeof(struct scp_hdr));

        if (SEQ_GT(p_end, seq)) {
            /* prev_end > seq，说明新段头部与 prev 重叠 */

            if (SEQ_GEQ(p_end, end)) {
                /* prev 完全覆盖 [seq, end)，新段是纯重复，丢弃 */
                scp_buf_free(sb);
                return;
            }

            /* 裁掉新段头部与 prev 重叠的部分 */
            uint32_t trim = p_end - seq;
            seq         += trim;
            payload_len -= trim;

            uint8_t *payload = sb->data + sizeof(struct scp_hdr);
            memmove(payload, payload + trim, payload_len);

            sh->seq = htonl(seq);
            sh->len = htons((uint16_t)payload_len);

            sb->seq = seq;
            sb->len = sizeof(struct scp_hdr) + payload_len;

            end = seq + payload_len;
        }
    }

    /* 4.2 右侧重叠：看插入位置 pos 本身是否与新段尾部重叠 */
    if (pos != &s->rcv_buf_q) {
        b = container_of(pos, struct scp_buf, node);
        uint32_t b_seq = b->seq;
        uint32_t b_end = b_seq + (b->len - sizeof(struct scp_hdr));

        if (SEQ_GT(b_seq, seq) && SEQ_GT(end, b_seq)) {
            /* 新段尾部与右邻居 b 重叠，只裁剪新段尾部 */

            uint32_t new_len = b_seq - seq;
            if (new_len == 0) {
                /* 新段完全落在 b 里面，纯重复，丢弃 */
                scp_buf_free(sb);
                return;
            }

            payload_len = new_len;
            sh->len     = htons((uint16_t)payload_len);
            sb->len     = sizeof(struct scp_hdr) + payload_len;
            end         = seq + payload_len;
        }
    }

    /* 4.3 经过裁剪后，如果 payload_len 变成 0，就没必要插入了 */
    if (payload_len == 0) {
        scp_buf_free(sb);
        return;
    }

    /* 4.4 插入到 rcv_buf_q 中（此时保证不与左右邻居重叠） */
    list_add_prev(pos, &sb->node);
    s->sb_cc += payload_len;
    scp_update_rcv_wnd(s);

    scp_output(s, SCP_FLAG_ACK);
}

void scp_snd_buf_free(struct scp_stream *ss, uint32_t ack)
{
    struct list_node *cur = ss->snd_q.next;

    while (cur != &ss->snd_q) {
        struct scp_buf *sb = container_of(cur, struct scp_buf, node);
        struct list_node *next = cur->next;

        uint32_t payload_len = sb->len - sizeof(struct scp_hdr);
        uint32_t end_seq     = sb->seq + payload_len;

        if (SEQ_LEQ(end_seq, ack)) {
            //printf("### SND_FREE: seq=%u end=%u ack=%u\n", sb->seq, end_seq, ack);
            list_remove(cur);
            scp_buf_free(sb);
            cur = next;
            continue;
        }

        break;
    }
}

/*
 * snd_una <= ack <= snd_nxt
 */
static void scp_process_ack(struct scp_stream *ss, uint32_t ack, uint32_t wnd, uint32_t timestamp)
{
    /* 1. 基本合法性检查 */
    if (SEQ_LT(ack, ss->snd_una) || SEQ_GT(ack, ss->snd_nxt)) {
        /*
        printf("### ACK_DROP: ack=%u snd_una=%u snd_nxt=%u\n",
               ack, ss->snd_una, ss->snd_nxt);
        */
        return;
    }

    uint32_t old_una = ss->snd_una;

    /* 2. RTT 采样（Karn） */
    if (ss->rtt_ts != 0 && SEQ_GEQ(ack, ss->rtt_seq)) {
        uint32_t sample = scp_clock - ss->rtt_ts;
        if (sample == 0) sample = 1;
        scp_update_rtt(ss, sample);
        ss->rtt_ts = 0;
        /*
        printf("### RTT_SAMPLE: sample=%u rto=%u srtt=%u rttvar=%u\n",
               sample, ss->rto, ss->srtt, ss->rttvar);
        */
    }

    /* 3. 更新 snd_una / 对端窗口 */
    ss->snd_una = ack;
    ss->snd_wnd = wnd;

    /* 4. 释放已确认的发送缓冲 */
    scp_snd_buf_free(ss, ack);

    /* 5. 有新数据被确认：正常前进路径 */
    if (SEQ_GT(ss->snd_una, old_una)) {
        /*
        printf("### ACK_ADV: old_una=%u new_una=%u snd_nxt=%u wnd=%u\n",
               old_una, ss->snd_una, ss->snd_nxt, ss->snd_wnd);
        */
        ss->timeout_count = 0;

        if (ss->snd_una == ss->snd_nxt) {
            /* 所有已发送数据都确认了，没有飞行数据 */
            ss->timer[TIMER_RETRANS] = 0;
            /*
            printf("### ACK_ALL_CONFIRMED: snd_una=%u snd_nxt=%u\n",
                   ss->snd_una, ss->snd_nxt);
            */
        } else {
            /* 仍有未确认数据，保持/重启 RTO 计时 */
            ss->timer[TIMER_RETRANS] = ss->rto;
            /*
            printf("### ACK_PARTIAL: snd_una=%u snd_nxt=%u rto=%u\n",
                   ss->snd_una, ss->snd_nxt, ss->rto);
            */
        }
    } else {
        /*
         * 6. 这里是“重复 ACK”路径：ack == old_una
         */

        /*
        printf("### ACK_DUP: ack=%u snd_una=%u snd_nxt=%u wnd=%u timer_retrans=%u\n",
               ack, ss->snd_una, ss->snd_nxt, ss->snd_wnd, ss->timer[TIMER_RETRANS]);
        */
        if (SEQ_LT(ss->snd_una, ss->snd_nxt) &&
            ss->timer[TIMER_RETRANS] == 0) {
/*
            printf("### FAST_RETX_TRIGGER: snd_una=%u snd_nxt=%u\n",
                   ss->snd_una, ss->snd_nxt);
*/
            /* 直接重传所有未确认数据 */
            scp_retransmit(ss);

            /* 重传后启动 RTO 计时 */
            ss->timeout_count = 1;
            ss->timer[TIMER_RETRANS] = ss->rto;
        }
    }

    /* 7. 零窗口处理：只影响“新数据发送”和 PERSIST，不影响重传 */
    if (wnd == 0) {
        //printf("### ACK_ZERO_WND: snd_una=%u snd_nxt=%u wnd=%u zero_wnd=%d\n",
               //ss->snd_una, ss->snd_nxt, wnd, ss->zero_wnd);

        if (!ss->zero_wnd) {
            ss->zero_wnd = 1;
            ss->persist_backoff = 5;
            ss->timer[TIMER_PERSIST] = ss->persist_backoff;
            //printf("### PERSIST_START: backoff=%u\n", ss->persist_backoff);
        }
    } else {
        /* 窗口重新打开 */
        int was_zero = ss->zero_wnd;
        ss->zero_wnd = 0;
        ss->persist_backoff = 5;
        ss->timer[TIMER_PERSIST] = 0;
/*
        printf("### WND_OPEN: wnd=%u was_zero=%d snd_una=%u snd_nxt=%u\n",
               wnd, was_zero, ss->snd_una, ss->snd_nxt);
*/
        if (ss->snd_una != ss->snd_nxt && ss->timer[TIMER_RETRANS] == 0) {
            ss->timer[TIMER_RETRANS] = ss->rto;
            ss->timeout_count = 0;
            //printf("### RETRANS_TIMER_START_ON_WND_OPEN: rto=%u\n", ss->rto);
        }
    }
}

void scp_output_ack(struct scp_stream *ss)
{
    struct scp_hdr *sh = malloc(sizeof(struct scp_hdr));
    if (!sh) return;

    memset(sh, 0, sizeof(struct scp_hdr));

    sh->seq = htonl(ss->snd_nxt);
    sh->ack = htonl(ss->rcv_nxt);
    sh->wnd = htons((uint16_t)ss->rcv_wnd);
    sh->len = 0;
    sh->flags = SCP_FLAG_ACK;
    sh->fd = ss->dst_fd;
    sh->cksum = in_checksum(sh, sizeof(struct scp_hdr));

    scp_debug_dump_tx("ACK", sh, sizeof(struct scp_hdr));
    scp_dump_hdr(ss, "ACK", sh);
    ss->st_class->send(ss->st_class->user, sh, sizeof(struct scp_hdr));

    free(sh);
}

void scp_output_data(struct scp_stream *ss, struct scp_buf *sb,
                     uint32_t offset, uint32_t frag_len)
{
    uint32_t pkt_len = sizeof(struct scp_hdr) + frag_len;

    uint8_t small_buf[64];
    uint8_t *pkt;

    if (pkt_len <= sizeof(small_buf)) {
        pkt = small_buf;
    } else {
        pkt = malloc(pkt_len);
        if (!pkt)
            return;
    }
    uint32_t seq = sb->seq + offset;       

    struct scp_hdr hdr;
    uint8_t *payload_base = sb->data + sizeof(struct scp_hdr);
    uint8_t *frag_payload = payload_base + offset;

    hdr.seq   = htonl(seq);
    hdr.ack   = htonl(ss->rcv_nxt);
    hdr.wnd   = htons((uint16_t)ss->rcv_wnd);
    hdr.len   = htons((uint16_t)frag_len);
    hdr.cksum = 0;
    hdr.flags = SCP_FLAG_DATA;
    hdr.fd    = ss->dst_fd;

    memcpy(pkt, &hdr, sizeof(struct scp_hdr));
    memcpy(pkt + sizeof(struct scp_hdr), frag_payload, frag_len);

    hdr.cksum = in_checksum(pkt, pkt_len);
    memcpy(pkt, &hdr, sizeof(struct scp_hdr));

    scp_debug_dump_tx("DATA", pkt, pkt_len);
    scp_dump_hdr(ss, "DATA", &hdr);
    ss->st_class->send(ss->st_class->user, pkt, pkt_len);

    if (pkt != small_buf)
        free(pkt);
}

static int scp_output(struct scp_stream *ss, int flags)
{
    if (flags == SCP_FLAG_ACK) {
        scp_output_ack(ss);
        return 0;
    }

    if (ss->rtt_ts == 0 && SEQ_EQ(ss->snd_una, ss->snd_nxt)) {
        ss->rtt_ts  = scp_clock;
        ss->rtt_seq = ss->snd_nxt;
    }

    int sent_any = 0;

    struct list_node *n;
    for (n = ss->snd_q.next; n != &ss->snd_q; n = n->next) {

        struct scp_buf *sb = container_of(n, struct scp_buf, node);

        uint32_t total = sb->len - sizeof(struct scp_hdr);
        uint32_t sent  = sb->sent_off;

        if (sent >= total) {
            continue;
        }

        while (sent < total) {
            uint32_t remain = total - sent;

            int64_t flight = 0;
            struct list_node *fn;
            for (fn = ss->snd_q.next; fn != &ss->snd_q; fn = fn->next) {
                struct scp_buf *fb = container_of(fn, struct scp_buf, node);

                uint32_t plen  = fb->len - sizeof(struct scp_hdr);
                uint32_t start = fb->seq;
                uint32_t end   = fb->seq + plen;

                if (SEQ_LEQ(end, ss->snd_una))
                    continue;

                if (SEQ_LT(start, ss->snd_una)) {
                    flight += end - ss->snd_una;
                } else {
                    flight += plen;
                }
            }

            if (ss->zero_wnd || ss->snd_wnd == 0) {
                goto out;
            }

            int32_t swnd = (int32_t)ss->snd_wnd - (int32_t)flight;
            if (swnd <= 0) {
                goto out;
            }

            uint32_t frag_len = min((uint32_t)(MTU - sizeof(struct scp_hdr)), remain);
            frag_len = min(frag_len, (uint32_t)swnd);
            if (frag_len == 0) {
                goto out;
            }

            scp_output_data(ss, sb, sent, frag_len);

            sb->sent_off += frag_len;
            sent         += frag_len;
            sent_any      = 1;
        }
    }

out:
    if (sent_any && ss->timer[TIMER_RETRANS] == 0) {
        ss->timer[TIMER_RETRANS] = ss->rto;
        ss->timeout_count = 0;
    }

    return 0;
}

int scp_connect(int fd)
{
    struct scp_stream *ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);
    if (!ss || ss->state != SCP_CLOSED)
        return -1;

    ss->iss     = random32();
    ss->snd_nxt = ss->iss;
    ss->snd_una = ss->iss;

    ss->state   = SCP_SYN_SENT;
    ss->retry   = 0;

    scp_output_connect(ss);
    ss->hs_timer = ss->rto;

    return 0;
}


int scp_send(int fd, void *buf, size_t len)
{
    struct scp_stream *ss;

    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);
    if (!ss) return -1;

    if (ss->state != SCP_ESTABLISHED) return -1;

    uint32_t seq_base = ss->snd_nxt; 
    uint32_t flight = ss->snd_nxt - ss->snd_una;  
    if (flight >= ss->snd_wnd) {
        return -2; // EWOULDBLOCK
    }

    ss->snd_nxt += len;

    struct scp_buf *sb = scp_buf_alloc(sizeof(struct scp_buf) + sizeof(struct scp_hdr) + len);
    sb->data = (uint8_t *)sb + sizeof(struct scp_buf);
    sb->len = sizeof(struct scp_hdr) + len;

    sb->seq = seq_base;

    //copy or not copy, this is a question.
    uint8_t *pure_data = (uint8_t *)sb->data + sizeof(struct scp_hdr);
    memcpy(pure_data, buf, len);
    queue_enqueue(&ss->snd_q, &sb->node);
    
    scp_output(ss, SCP_FLAG_DATA);

    return 0;
}

static void scp_close_process(struct scp_stream *ss,
                              struct scp_hdr *sh,
                              struct scp_buf *sb,
                              uint32_t ack,
                              uint32_t wnd)
{
    if (sh->flags & SCP_FLAG_CONNECT) {
        ss->state = SCP_SYN_RECV;

        ss->irs     = ntohl(sh->seq);
        ss->rcv_nxt = ss->irs;

        ss->iss     = random32();
        ss->snd_nxt = ss->iss;
        ss->snd_una = ss->iss;

        scp_output_connect_ack(ss);
        ss->hs_timer = ss->rto;
    }

    scp_buf_free(sb);
}
                                               

static void scp_syn_sent_process(struct scp_stream *ss,
                                 struct scp_hdr *sh,
                                 struct scp_buf *sb,
                                 uint32_t ack,
                                 uint32_t wnd)
{
    if (sh->flags & SCP_FLAG_CONNECT_ACK) {
        ss->irs = ntohl(sh->seq);
        ss->rcv_nxt = ss->irs;

        scp_output_ack(ss);

        ss->state = SCP_ESTABLISHED;
        ss->hs_timer = 0;
    }

    scp_buf_free(sb);
}

static void scp_syn_recv_process(struct scp_stream *ss,
                                 struct scp_hdr *sh,
                                 struct scp_buf *sb,
                                 uint32_t ack,
                                 uint32_t wnd)
{
    if (sh->flags & SCP_FLAG_ACK) {
        ss->state = SCP_ESTABLISHED;
        ss->hs_timer = 0;

        if (sh->flags & SCP_FLAG_DATA) {
            scp_process_data(ss, sb);
            return;
        }

        scp_buf_free(sb);
        return;
    }

    if (sh->flags & SCP_FLAG_DATA) {
        ss->state = SCP_ESTABLISHED;
        ss->hs_timer = 0;
        scp_process_data(ss, sb);
        return;
    }

    scp_buf_free(sb);
}

static void scp_est_process(struct scp_stream *ss, struct scp_hdr *sh, struct scp_buf *sb,
                            uint32_t ack,
                            uint32_t wnd)
{
    if (sh->flags & SCP_FLAG_FIN) {
        uint32_t seq = ntohl(sh->seq);

        if (SEQ_LT(seq, ss->rcv_nxt)) {
            scp_output_ack(ss);
            scp_buf_free(sb);
            return;
        }

        ss->rcv_nxt = seq;   
        scp_output_ack(ss);

        ss->state     = SCP_LAST_ACK;
        ss->retry     = 0;
        ss->fin_timer = ss->rto;
        scp_output_fin(ss);

        scp_buf_free(sb);
        return;
    }

    if (sh->flags & SCP_FLAG_DATA) {
        scp_process_data(ss, sb);
        return;
    } else if (sh->flags & SCP_FLAG_ACK) {
        scp_process_ack(ss, ack, wnd, scp_clock);
    } else if (sh->flags & SCP_FLAG_PING) {
        scp_process_ack(ss, ack, wnd, scp_clock);
    }
    scp_buf_free(sb);
}

static void scp_fin_wait_process(struct scp_stream *ss, struct scp_hdr *sh, struct scp_buf *sb,
                                 uint32_t ack,
                                 uint32_t wnd)
{
    if (sh->flags & (SCP_FLAG_ACK | SCP_FLAG_PING)) {
        scp_process_ack(ss, ack, wnd, scp_clock);
    }

    if (sh->flags & SCP_FLAG_DATA) {
        scp_output_ack(ss);
        scp_buf_free(sb);
        return;
    }

    if (sh->flags & SCP_FLAG_FIN) {
        uint32_t seq = ntohl(sh->seq);
        if (SEQ_GEQ(seq, ss->rcv_nxt)) {
            ss->rcv_nxt = seq;
        }

        scp_output_ack(ss);

        ss->state     = SCP_CLOSED;
        ss->fin_timer = 0;
        scp_buf_free(sb);
        scp_stream_free(ss);
        return;
    }

    scp_buf_free(sb);
}                            

static void scp_close_wait_process(struct scp_stream *ss,
                                   struct scp_hdr *sh,
                                   struct scp_buf *sb,
                                   uint32_t ack,
                                   uint32_t wnd)
{
    if (sh->flags & (SCP_FLAG_DATA | SCP_FLAG_FIN)) {
        scp_output_ack(ss);
    }
    scp_buf_free(sb);
}

static void scp_last_ack_process(struct scp_stream *ss,
                                 struct scp_hdr *sh,
                                 struct scp_buf *sb,
                                 uint32_t ack,
                                 uint32_t wnd)
{
    if (sh->flags & (SCP_FLAG_ACK | SCP_FLAG_PING)) {
        scp_process_ack(ss, ack, wnd, scp_clock);
    }

    if (sh->flags & SCP_FLAG_FIN) {
        scp_output_ack(ss);
        scp_buf_free(sb);
        return;
    }

    if (sh->flags & SCP_FLAG_ACK) {
        ss->state     = SCP_CLOSED;
        ss->fin_timer = 0;
        scp_buf_free(sb);
        scp_stream_free(ss);
        return;
    }

    scp_buf_free(sb);
}

int scp_input(void *ctx, void *buf, size_t len)
{
    struct scp_buf *sb;
    struct scp_hdr *sh;
    struct scp_stream *ss;

    scp_debug_dump_rx(buf, len);

    sb = scp_buf_alloc(sizeof(struct scp_buf) + len);
    if (!sb) return -1;

    memcpy(sb->data, buf, len);
    sb->len = len;

    sh = (struct scp_hdr *)sb->data;

    uint16_t calc = in_checksum(buf, len);
    if (calc != 0) {
        scp_buf_free(sb);
        return -1;
    }

    uint32_t ack = ntohl(sh->ack);
    uint32_t wnd = ntohs(sh->wnd);

    ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)sh->fd);
    if (!ss) {
        scp_buf_free(sb);
        return -1;
    }

    switch (ss->state) {
    case SCP_CLOSED:
        scp_close_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_SYN_SENT:
        scp_syn_sent_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_SYN_RECV:
        scp_syn_recv_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_ESTABLISHED:
        scp_est_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_FIN_WAIT:
        scp_fin_wait_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_CLOSE_WAIT:
        scp_close_wait_process(ss, sh, sb, ack, wnd);
        break;
    case SCP_LAST_ACK:
        scp_last_ack_process(ss, sh, sb, ack, wnd);
        break;
    default:
        scp_buf_free(sb);
        break;
    }

    return 0;
}

void scp_close(int fd)
{
    struct scp_stream *ss = hashmap_get(&scp_stream_map, (void *)(uintptr_t)fd);
    if (!ss) return;

    switch (ss->state) {
    case SCP_ESTABLISHED:
        ss->state     = SCP_FIN_WAIT;
        ss->retry     = 0;
        ss->fin_timer = ss->rto;
        scp_output_fin(ss);
        break;

    case SCP_SYN_SENT:
    case SCP_SYN_RECV:
        ss->state = SCP_CLOSED;
        scp_stream_free(ss);
        break;

    case SCP_FIN_WAIT:
    case SCP_CLOSE_WAIT:
    case SCP_LAST_ACK:
        break;

    case SCP_CLOSED:
    default:
        scp_stream_free(ss);
        break;
    }
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

        uint32_t total_payload = sb->len - sizeof(struct scp_hdr);
        uint32_t payload_len   = total_payload - sb->payload_off;

        uint32_t take = (uint32_t)min(len - copied, payload_len);
        uint8_t *payload = sb->data + sizeof(struct scp_hdr) + sb->payload_off;

        memcpy(dst + copied, payload, take);
        copied += take;

        if (take == payload_len) {
            list_remove(n);
            scp_buf_free(sb);
        } else {
            sb->payload_off += take;
        }
    }

    /* 
    if (copied > 0) {
        int rcv_buf_q_len = 0;
        struct list_node *p;
        for (p = s->rcv_buf_q.next; p != &s->rcv_buf_q; p = p->next)
            rcv_buf_q_len++;

        int rcv_data_q_len = 0;
        for (p = s->rcv_data_q.next; p != &s->rcv_data_q; p = p->next)
            rcv_data_q_len++;

        printf("### RECV_APP: fd=%d copied=%zu sb_cc=%u rcv_wnd=%u "
               "rcv_buf_q=%d rcv_data_q=%d rcv_nxt=%u\n",
               fd, copied, s->sb_cc, s->rcv_wnd,
               rcv_buf_q_len, rcv_data_q_len, s->rcv_nxt);
    }
   */

    if (copied > s->sb_cc) {
        s->sb_cc = 0;
    } else {
        s->sb_cc -= copied;
    }

    uint32_t old_wnd = s->rcv_wnd;
    scp_update_rcv_wnd(s);

    if (old_wnd == 0 && s->rcv_wnd > 0) {
        scp_output(s, SCP_FLAG_ACK);
    }

    return copied;
}
