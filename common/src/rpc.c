#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "hashmap.h"
#include "rpc.h"


struct pending_call {
    uint32_t      seq;
    uint8_t      *buf;       // 响应 TLV 存放地址（由调用方提供） 
    size_t        buf_size;  // buf 的最大长度 
    size_t        resp_len;  
    rpc_status_t  status;
    int           done;
};


struct rpc_method_entry {
    const char *name;
    rpc_param_parser_t   parse_param;
    rpc_handler_t        handler;
    rpc_result_encoder_t encode_result;
};


static struct hashmap g_methods;
static rpc_transport_t *g_transport = NULL;

static uint32_t g_next_seq = 1;
static struct hashmap g_pending;

#ifndef RPC_MAX_PARAM_SIZE
#define RPC_MAX_PARAM_SIZE   256
#endif

#ifndef RPC_MAX_RESULT_SIZE
#define RPC_MAX_RESULT_SIZE  256
#endif

#ifndef RPC_MAX_RESULT_TLV_SIZE
#define RPC_MAX_RESULT_TLV_SIZE 512
#endif

static uint8_t param_buf[RPC_MAX_PARAM_SIZE];
static uint8_t result_buf[RPC_MAX_RESULT_SIZE];

static char    method[128];
static uint8_t resp_tlv[RPC_MAX_RESULT_TLV_SIZE];

// max tcp segment
static uint8_t poll_buf[1500];

static uint32_t rpc_next_seq(void) {
    uint32_t s = g_next_seq++;
    if (g_next_seq == 0)
        g_next_seq = 1;
    return s;
}

static int rpc_send_response_tlv(uint32_t seq,
                                 rpc_status_t status,
                                 const uint8_t *tlv, size_t tlv_len)
{
    size_t total;
    struct rpc_message *msg;
    int ret;
    
    total = sizeof(struct rpc_header) + tlv_len;

    msg = malloc(total);
    if (!msg)
        return -1;

    msg->hdr.version    = htons(RPC_VERSION_2_0);
    msg->hdr.flags      = htons(RPC_FLAG_RESPONSE |
                                (status != RPC_STATUS_OK ? RPC_FLAG_ERROR : 0));
    msg->hdr.seq        = htonl(seq);
    msg->hdr.method_len = htons(0);
    msg->hdr.status     = htons((uint16_t)status);

    if (tlv_len > 0 && tlv)
        memcpy(msg->payload, tlv, tlv_len);

    ret = g_transport->send(g_transport->user,
                              (const uint8_t *)msg, total);
    free(msg);
    return ret;
}

static int rpc_send_request_tlv(uint32_t seq,
                                const char *method,
                                const uint8_t *tlv, size_t tlv_len)
{
    size_t mlen;
    size_t total;
    int ret;

    mlen = strlen(method);
    total = sizeof(struct rpc_header) + mlen + tlv_len;

    struct rpc_message *msg = malloc(total);
    if (!msg)
        return -1;

    msg->hdr.version    = htons(RPC_VERSION_2_0);
    msg->hdr.flags      = htons(0); // request 
    msg->hdr.seq        = htonl(seq);
    msg->hdr.method_len = htons((uint16_t)mlen);
    msg->hdr.status     = htons(0);

    memcpy(msg->payload, method, mlen);
    if (tlv_len > 0 && tlv)
        memcpy(msg->payload + mlen, tlv, tlv_len);

    ret = g_transport->send(g_transport->user,
                              (const uint8_t *)msg, total);
    free(msg);
    return ret;
}

static void rpc_handle_request(const struct rpc_message *msg, size_t len)
{
    size_t tmp;               
    uint32_t seq;
    uint16_t method_len;
    const uint8_t *tlv;
    struct rpc_method_entry *m;
    rpc_status_t status;
    int rc;                   

    if (len < sizeof(struct rpc_header))
        return;

    if (ntohs(msg->hdr.version) != RPC_VERSION_2_0)
        return;

    seq        = ntohl(msg->hdr.seq);
    method_len = ntohs(msg->hdr.method_len);

    tmp = len - sizeof(struct rpc_header);  
    if (tmp < method_len)
        return;

    if (method_len == 0 || method_len >= sizeof(method))
        return;

    memcpy(method, msg->payload, method_len);
    method[method_len] = '\0';

    tlv     = msg->payload + method_len;
    tmp    -= method_len;                

    m = hashmap_get(&g_methods, (void *)method);
    if (!m) {
        rpc_send_response_tlv(seq, RPC_STATUS_METHOD_NOT_FOUND, NULL, 0);
        return;
    }

    status = RPC_STATUS_OK;

    memset(param_buf,  0, sizeof(param_buf));
    memset(result_buf, 0, sizeof(result_buf));

    if (m->parse_param) {
        rc = m->parse_param(tlv, tmp, param_buf);
        if (rc != 0)
            status = RPC_STATUS_INVALID_PARAMS;
    }

    if (status == RPC_STATUS_OK && m->handler) {
        rc = m->handler(param_buf, result_buf);
        if (rc != 0)
            status = RPC_STATUS_INTERNAL_ERROR;
    }

    size_t resp_len = 0;
    memset(resp_tlv, 0, sizeof(resp_tlv));

    if (status == RPC_STATUS_OK && m->encode_result) {
        rc = m->encode_result(result_buf, resp_tlv, &resp_len);
        if (rc != 0) {
            status = RPC_STATUS_INTERNAL_ERROR;
            resp_len = 0;
        }
    }

    rpc_send_response_tlv(seq,
                          status,
                          (status == RPC_STATUS_OK && resp_len > 0) ? resp_tlv : NULL,
                          (status == RPC_STATUS_OK) ? resp_len : 0);
}

static void rpc_handle_response(const struct rpc_message *msg, size_t len)
{
    size_t tmp;                  
    uint32_t seq;
    uint16_t status;
    struct pending_call *pc;

    if (len < sizeof(struct rpc_header))
        return;

    if (ntohs(msg->hdr.version) != RPC_VERSION_2_0)
        return;

    seq    = ntohl(msg->hdr.seq);
    status = ntohs(msg->hdr.status);

    pc = hashmap_get(&g_pending, (void *)(uintptr_t)seq);
    if (!pc)
        return;

    tmp = len - sizeof(struct rpc_header); 

    if (tmp > pc->buf_size)
        tmp = pc->buf_size;             

    if (pc->buf && tmp > 0)
        memcpy(pc->buf, msg->payload, tmp);

    pc->resp_len = tmp;
    pc->status   = (rpc_status_t)status;
    pc->done     = 1;
}



void rpc_init(void) {
    hashmap_init(&g_methods,  64, HASHMAP_KEY_STRING);
    hashmap_init(&g_pending,  64, HASHMAP_KEY_INT);
    g_transport = NULL;
    g_next_seq  = 1;
}

void rpc_set_transport(rpc_transport_t *t) {
    g_transport = t;
}


int rpc_call_with_tlv(const char *method,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *resp_tlv, size_t *resp_len)
{
    uint32_t seq;
    if (!g_transport)
        return -RPC_STATUS_TRANSPORT_ERROR;

    seq = rpc_next_seq();

    struct pending_call pc = {
        .seq      = seq,
        .buf      = resp_tlv,
        .buf_size = resp_len ? *resp_len : 0,
        .resp_len = 0,
        .status   = RPC_STATUS_OK,
        .done     = 0
    };

    hashmap_put(&g_pending, (void *)(uintptr_t)seq, &pc);

    int r = rpc_send_request_tlv(seq, method, tlv, tlv_len);
    if (r < 0) {
        hashmap_remove(&g_pending, (void *)(uintptr_t)seq);
        return -RPC_STATUS_TRANSPORT_ERROR;
    }

    while (!pc.done)
        rpc_poll();

    hashmap_remove(&g_pending, (void *)(uintptr_t)seq);

    if (resp_tlv && resp_len)
        *resp_len = pc.resp_len;

    return (int)pc.status;
}

void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder)
{
    struct rpc_method_entry *e = malloc(sizeof(*e));
    if (!e)
        return;

    e->name          = name;
    e->parse_param   = parser;
    e->handler       = handler;
    e->encode_result = encoder;

    hashmap_put(&g_methods, (void *)name, e);
}

void rpc_poll(void)
{
    ssize_t n;
    struct rpc_message *msg;
    uint16_t flags;
    uint16_t method_len;

    if (!g_transport)
        return;

    n = g_transport->recv(g_transport->user, poll_buf, sizeof(poll_buf));
    if (n <= 0)
        return;

    if (n < sizeof(struct rpc_header))
        return;

    msg = (struct rpc_message *)poll_buf;

    flags      = ntohs(msg->hdr.flags);
    method_len = ntohs(msg->hdr.method_len);

    if ((flags & RPC_FLAG_RESPONSE) != 0 || method_len == 0) {
        rpc_handle_response(msg, (size_t)n);
    } else {
        rpc_handle_request(msg, (size_t)n);
    }
}
