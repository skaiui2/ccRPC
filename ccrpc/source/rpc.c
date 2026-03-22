#include <string.h>
#include <stdio.h>
#include "hashmap.h"
#include "common.h"
#include "rpc.h"
#include "rpc_port.h"
#include "heap.h"

struct pending_call {
    uint32_t      seq;
    uint8_t      *buf;
    size_t        buf_size;
    size_t        resp_len;
    rpc_status_t  status;
    int           done;
    struct rpc_waiter *waiter;
};

struct rpc_method_entry {
    const char *name;
    rpc_param_parser_t   parse_param;
    rpc_handler_t        handler;
    rpc_result_encoder_t encode_result;
    rpc_free_param_t     free_param;
    rpc_free_param_t     free_result;
};

struct rpc_transport_entry {
    const char     *name;
    struct rpc_transport_class *transport;
};

static struct hashmap g_methods;

static uint32_t g_next_seq = 1;
static struct hashmap g_pending;
static struct hashmap g_transport_map;

#define RPC_MAX_PARAM_SIZE   4096
#define RPC_MAX_RESULT_SIZE  4096
#define RPC_MAX_RESULT_TLV_SIZE 4096

static uint8_t param_buf[RPC_MAX_PARAM_SIZE];
static uint8_t result_buf[RPC_MAX_RESULT_SIZE];

#define METHOD_LEN 128
static char    method[METHOD_LEN];
static uint8_t resp_tlv[RPC_MAX_RESULT_TLV_SIZE];

// max segment
static uint8_t poll_buf[MTU];

#define RPC_DEBUG
static void ccnet_debug_hex(const char *tag, const void *buf, size_t len)
{
#ifndef RPC_DEBUG
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

static int rpc_tx_req_id = 0;

void rpc_debug_dump_tx_request(const char *name,
                               uint32_t seq,
                               const void *buf, size_t len)
{
#ifndef RPC_DEBUG
    printf("\n[RPC TX] REQUEST name=%s seq=%u id:%d\n",
           name, seq, rpc_tx_req_id++);
#endif
}

static int rpc_tx_resp_id = 0;

void rpc_debug_dump_tx_response(uint32_t seq,
                                uint16_t status,
                                const void *buf, size_t len)
{
#ifndef RPC_DEBUG
    printf("\n[RPC TX] RESPONSE seq=%u status=%u id:%d\n",
           seq, status, rpc_tx_resp_id++);
    ccnet_debug_hex("RPC Response", buf, len);
#endif
}

static int rpc_rx_id = 0;
void rpc_debug_dump_rx(const void *buf, size_t len)
{
#ifndef RPC_DEBUG
    printf("\n[RPC RX] id:%d\n", rpc_rx_id++);
    ccnet_debug_hex("RPC RX", buf, len);
#endif
}

static uint32_t rpc_next_seq(void)
{
    uint32_t s;

    do {
        s = g_next_seq++;
        if (g_next_seq == 0)
            g_next_seq = 1;
    } while (hashmap_contains(&g_pending, (void*)(uintptr_t)s));

    return s;
}

static struct rpc_buf *rpc_buf_alloc(const uint8_t *data, size_t len)
{
    struct rpc_buf *b = heap_malloc(sizeof(*b) + len);
    if (!b)
        return NULL;

    list_node_init(&b->node);
    b->len = len;

    if (len > 0 && data)
        memcpy(b->data, data, len);

    return b;
}

static void rpc_reasm_append(struct rpc_reasm *r, const uint8_t *data, size_t len)
{
    struct rpc_buf *b;

    if (!r || len == 0)
        return;

    b = rpc_buf_alloc(data, len);
    if (!b)
        return;

    list_add_prev(&r->head, &b->node);
    r->total_len += len;
}

/*
 * Get a header, if the rpc_buf->len >= need,
 * we can get a header.Other, find next.
*/
static int rpc_reasm_peek_header(struct rpc_reasm *r, struct rpc_header *hdr)
{
    size_t copied_size = 0;
    size_t remain_size = 0;
    size_t take = 0;
    struct rpc_buf *b;
    struct list_node *pos;

    if (!r || !hdr)
        return -1;

    if (r->total_len < sizeof(*hdr))
        return -1;

    pos = r->head.next;

    while (pos != &r->head && copied_size < sizeof(*hdr)) {
        b = container_of(pos, struct rpc_buf, node);
        remain_size = sizeof(*hdr) - copied_size;
        if (remain_size > b->len) {
            take = b->len;
        } else {
            take = remain_size;
        }

        memcpy((uint8_t *)hdr + copied_size, b->data, take);
        copied_size += take;

        pos = pos->next;
    }

    return (copied_size == sizeof(*hdr)) ? 0 : -1;
}

static void rpc_reasm_consume(struct rpc_reasm *r, uint8_t *out, size_t len)
{
    size_t copied = 0;
    size_t remain;
    size_t take;
    struct rpc_buf *b;
    struct list_node *pos, *n;

    if (!r || len == 0)
        return;

    pos = r->head.next;

    while (pos != &r->head && copied < len) {
        b = container_of(pos, struct rpc_buf, node);
        n = pos->next;

        remain = len - copied;
        take = b->len < remain ? b->len : remain;

        if (out)
            memcpy(out + copied, b->data, take);

        copied += take;

        if (take == b->len) {
            list_remove(&b->node);
            heap_free(b);
        } else {
            memmove(b->data, b->data + take, b->len - take);
            b->len -= take;
            break;
        }

        pos = n;
    }

    r->total_len -= copied;
}

struct rpc_transport_class *rpc_trans_class_create(void *send, void *recv, void *close, void *user)
{
    struct rpc_transport_class *ret = heap_malloc(sizeof(struct rpc_transport_class)); // fix
    if (!ret)
        return NULL;

    ret->user  = user;
    ret->send  = send;
    ret->recv  = recv;
    ret->close = close;

    list_node_init(&ret->reasm.head);
    ret->reasm.total_len = 0;

    return ret;
}

static int rpc_send_response_tlv(struct rpc_transport_class *t,
                                 uint32_t seq,
                                 rpc_status_t status,
                                 const uint8_t *tlv, size_t tlv_len)
{
    size_t total;
    struct rpc_message *msg;
    int ret;

    if (!t || !t->send)
        return -1;

    total = sizeof(struct rpc_header) + tlv_len;

    msg = heap_malloc(total);
    if (!msg)
        return -1;

    msg->hdr.status     = htons((uint16_t)status);
    msg->hdr.seq        = htonl(seq);
    msg->hdr.msg_len    = htonl((uint32_t)tlv_len);
    msg->hdr.method_len = htons(0);

    if (tlv_len > 0 && tlv)
        memcpy(msg->payload, tlv, tlv_len);

    rpc_debug_dump_tx_response(seq, status, msg, total);
    ret = (int)t->send(t->user, (const uint8_t *)msg, total);
    heap_free(msg);
    return ret;
}

static int rpc_send_request_tlv(struct rpc_transport_class *t,
                                uint32_t seq,
                                const char *name,
                                const uint8_t *tlv, size_t tlv_len)
{
    size_t mlen;
    size_t total;
    struct rpc_message *msg;
    int ret;

    if (!t || !t->send || !name)
        return -1;

    mlen  = strlen(name);
    total = sizeof(struct rpc_header) + mlen + tlv_len;

    msg = heap_malloc(total);
    if (!msg)
        return -1;

    msg->hdr.status     = htons((uint16_t)RPC_STATUS_OK);
    msg->hdr.seq        = htonl(seq);
    msg->hdr.msg_len    = htonl((uint32_t)(mlen + tlv_len));
    msg->hdr.method_len = htons((uint16_t)mlen);

    memcpy(msg->payload, name, mlen);
    if (tlv_len > 0 && tlv)
        memcpy(msg->payload + mlen, tlv, tlv_len);

    rpc_debug_dump_tx_request(name, seq, msg, total);
    ret = (int)t->send(t->user, (const uint8_t *)msg, total);
    heap_free(msg);
    return ret;
}

static void rpc_handle_request(struct rpc_transport_class *t,
                               const struct rpc_message *msg, size_t len)
{
    if (len < sizeof(struct rpc_header))
        return;

    const struct rpc_header *hdr = &msg->hdr;

    uint32_t seq        = ntohl(hdr->seq);
    uint32_t msg_len    = ntohl(hdr->msg_len);
    uint16_t method_len = ntohs(hdr->method_len);

    if (msg_len < method_len)
        return;

    if (method_len == 0 || method_len >= sizeof(method))
        return;

    if (len < sizeof(struct rpc_header) + msg_len)
        return;

    const uint8_t *payload = msg->payload;

    memcpy(method, payload, method_len);
    method[method_len] = '\0';

    const uint8_t *tlv = payload + method_len;

    size_t tlv_len = msg_len - method_len;

    struct rpc_method_entry *m = hashmap_get(&g_methods, (void *)method);
    if (!m) {
        rpc_send_response_tlv(t, seq, RPC_STATUS_METHOD_NOT_FOUND, NULL, 0);
        return;
    }

    rpc_status_t status = RPC_STATUS_OK;

    memset(param_buf,  0, sizeof(param_buf));
    memset(result_buf, 0, sizeof(result_buf));

    if (m->parse_param) {
        int rc = m->parse_param(tlv, tlv_len, param_buf);
        if (rc != 0)
            status = RPC_STATUS_INVALID_PARAMS;
    }

    if (status == RPC_STATUS_OK && m->handler) {
        int rc = m->handler(param_buf, result_buf);
        if (rc != 0)
            status = RPC_STATUS_INTERNAL_ERROR;
    }

    size_t resp_len = sizeof(resp_tlv);
    memset(resp_tlv, 0, sizeof(resp_tlv));

    if (status == RPC_STATUS_OK && m->encode_result) {
        int rc = m->encode_result(result_buf, resp_tlv, &resp_len);
        if (rc != 0) {
            status   = RPC_STATUS_INTERNAL_ERROR;
            resp_len = 0;
        }
    }

    rpc_send_response_tlv(
            t,
            seq,
            status,
            (status == RPC_STATUS_OK && resp_len > 0) ? resp_tlv : NULL,
            (status == RPC_STATUS_OK) ? resp_len : 0
    );

    if (m->free_param) {
        m->free_param(param_buf);
    }

    if (m->free_result) {
        m->free_result(result_buf);
    }

}

static void rpc_handle_response(const struct rpc_message *msg, size_t len)
{
    uint32_t seq;
    uint32_t msg_len;
    struct pending_call *pc;

    if (len < sizeof(struct rpc_header))
        return;

    seq = ntohl(msg->hdr.seq);
    msg_len = ntohl(msg->hdr.msg_len);

    if (len < sizeof(struct rpc_header) + msg_len)
        return;

    pc = hashmap_get(&g_pending, (void *)(uintptr_t)seq);
    if (!pc)
        return;

    size_t copy_len = msg_len;
    if (copy_len > pc->buf_size)
        copy_len = pc->buf_size;

    if (pc->buf && copy_len > 0)
        memcpy(pc->buf, msg->payload, copy_len);

    pc->resp_len = copy_len;
    pc->status   = (rpc_status_t)ntohs(msg->hdr.status);
    pc->done     = 1;
    rpc_waiter_wake(pc->waiter);
}

static void rpc_dispatch_message(struct rpc_transport_class *t,
                                 const uint8_t *buf, size_t len)
{
    rpc_debug_dump_rx(buf, len);

    const struct rpc_message *msg = (const struct rpc_message *)buf;
    uint16_t method_len;

    if (len < sizeof(struct rpc_header))
        return;

    method_len = ntohs(msg->hdr.method_len);
    if (method_len == 0)
        rpc_handle_response(msg, len);
    else
        rpc_handle_request(t, msg, len);
}

void rpc_on_data(struct rpc_transport_class *t,
                 const uint8_t *buf, size_t len)
{
    ssize_t n;
    struct rpc_reasm *r;
    struct rpc_header hdr;
    uint32_t msg_len;
    size_t need;
    uint8_t *msg_buf;

    if (!t || !t->recv)
        return;

    rpc_port_lock();

    r = &t->reasm;

    if (len > 0) {
        rpc_reasm_append(r, buf, len);
    } else {
        n = t->recv(t->user, poll_buf, sizeof(poll_buf));
        if (n > 0)
            rpc_reasm_append(r, poll_buf, (size_t)n);
        else
            return;
    }

    for (;;) {
        if (r->total_len < sizeof(struct rpc_header))
            break;

        if (rpc_reasm_peek_header(r, &hdr) < 0)
            break;

        msg_len = ntohl(hdr.msg_len);
        need    = sizeof(struct rpc_header) + (size_t)msg_len;

        if (r->total_len < need)
            break;

        msg_buf = heap_malloc(need);
        if (!msg_buf) {
            rpc_reasm_consume(r, NULL, need);
            continue;
        }

        rpc_reasm_consume(r, msg_buf, need);
        rpc_dispatch_message(t, msg_buf, need);
        heap_free(msg_buf);
    }

    rpc_port_unlock();
}

void rpc_init(uint8_t method_cap, uint8_t pending_cap, uint8_t transport_cap)
{
    hashmap_init(&g_methods,       method_cap,    HASHMAP_KEY_STRING);
    hashmap_init(&g_pending,       pending_cap,   HASHMAP_KEY_INT);
    hashmap_init(&g_transport_map, transport_cap, HASHMAP_KEY_STRING);
}

void rpc_bind_transport(const char *name, struct rpc_transport_class *t)
{
    struct rpc_transport_entry *e;

    if (!name || !t)
        return;

    e = heap_malloc(sizeof(*e));
    if (!e)
        return;

    e->name      = name;
    e->transport = t;

    rpc_port_lock();
    hashmap_put(&g_transport_map, (void *)name, e);
    rpc_port_unlock();
}

struct rpc_transport_class *rpc_transport_lookup(const char *name)
{
    struct rpc_transport_entry *e;

    if (!name)
        return NULL;

    e = hashmap_get(&g_transport_map, (void *)name);
    return e ? e->transport : NULL;
}

int rpc_call_with_tlv(const char *name,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *out_tlv, size_t *out_len,
                      uint32_t timeout_ms)
{
    uint32_t seq;
    struct rpc_transport_class *t;
    struct pending_call *pc;
    int r;

    t = rpc_transport_lookup(name);
    if (!t)
        return -RPC_STATUS_TRANSPORT_ERROR;

    rpc_port_lock();

    seq = rpc_next_seq();

    pc = heap_malloc(sizeof(struct pending_call));
    if (!pc) {
        rpc_port_unlock();
        return -RPC_STATUS_INTERNAL_ERROR;
    }

    pc->seq      = seq;
    pc->buf      = out_tlv;
    pc->buf_size = out_len ? *out_len : 0;
    pc->resp_len = 0;
    pc->status   = RPC_STATUS_OK;
    pc->done     = 0;
    pc->waiter   = rpc_waiter_create();

    if (!pc->waiter) {
        heap_free(pc);
        rpc_port_unlock();
        return -RPC_STATUS_INTERNAL_ERROR;
    }

    hashmap_put(&g_pending, (void *)(uintptr_t)seq, pc);

    r = rpc_send_request_tlv(t, seq, name, tlv, tlv_len);
    if (r < 0) {
        hashmap_remove(&g_pending, (void *)(uintptr_t)seq);
        rpc_waiter_destroy(pc->waiter);
        heap_free(pc);
        rpc_port_unlock();
        return -RPC_STATUS_TRANSPORT_ERROR;
    }

    rpc_port_unlock();

    int wait_ret = rpc_waiter_wait(pc->waiter, timeout_ms);

    rpc_port_lock();

    hashmap_remove(&g_pending, (void *)(uintptr_t)seq);

    if (out_tlv && out_len)
        *out_len = pc->resp_len;

    rpc_waiter_destroy(pc->waiter);

    int status = (wait_ret == 0) ? pc->status
                                 : RPC_STATUS_TIMEOUT;

    heap_free(pc);

    rpc_port_unlock();

    return status;
}

void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder,
                         rpc_free_param_t free_param,
                         rpc_free_param_t free_result)
{
    struct rpc_method_entry *e;

    if (!name)
        return;

    e = heap_malloc(sizeof(*e));
    if (!e)
        return;

    e->name          = name;
    e->parse_param   = parser;
    e->handler       = handler;
    e->encode_result = encoder;
    e->free_param = free_param;
    e->free_result = free_result;

    rpc_port_lock();
    hashmap_put(&g_methods, (void *)name, e);
    rpc_port_unlock();
}
