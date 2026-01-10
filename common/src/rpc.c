#include <string.h>
#include <stdlib.h>
#include "hashmap.h"
#include "rpc.h"
#include "rpc_method.h"
#include "rpc_result.h"

static struct hashmap g_methods;
static rpc_transport_t *g_transport = NULL;

/* Simple sync call state; single outstanding request at a time */
static uint16_t g_next_req_id     = 1;
static uint16_t g_waiting_req_id  = 0;
static int      g_response_ready  = 0;

static uint8_t g_response_buffer[512];
static size_t  g_response_len     = 0;

static uint16_t rpc_next_req_id(void) {
    return g_next_req_id++;
}

/* Decode raw bytes into rpc_message (shallow copy) */
static struct rpc_message *rpc_decode(const uint8_t *buf, size_t len) {
    if (len < sizeof(struct rpc_header))
        return NULL;

    struct rpc_message *msg = malloc(len);
    if (!msg)
        return NULL;

    memcpy(msg, buf, len);
    return msg;
}

/* Core send helpers (TLV payload only) */

static int rpc_send_response_tlv(uint16_t req_id,
                                 const uint8_t *tlv, size_t tlv_len)
{
    if (tlv_len > 255)
        return -1;

    size_t total = sizeof(struct rpc_header) + tlv_len;
    struct rpc_message *msg = malloc(total);
    if (!msg)
        return -1;

    msg->hdr.req_id  = req_id;
    msg->hdr.type    = RPC_MSG_RESPONSE;
    msg->hdr.reserved = (uint8_t)tlv_len;

    if (tlv_len > 0)
        memcpy(msg->payload, tlv, tlv_len);

    int r = g_transport->send(g_transport->user,
                              (const uint8_t *)msg, total);
    free(msg);
    return r;
}

static int rpc_send_request_tlv(uint16_t req_id,
                                const char *method,
                                const uint8_t *tlv, size_t tlv_len)
{
    if (tlv_len > 255)
        return -1;

    size_t mlen = strlen(method);
    size_t total_payload = mlen + 1 + tlv_len;
    size_t total = sizeof(struct rpc_header) + total_payload;

    struct rpc_message *msg = malloc(total);
    if (!msg)
        return -1;

    msg->hdr.req_id   = req_id;
    msg->hdr.type     = RPC_MSG_REQUEST;
    msg->hdr.reserved = (uint8_t)tlv_len;

    /* payload = method\0 + tlv */
    memcpy(msg->payload, method, mlen + 1);
    if (tlv_len > 0)
        memcpy(msg->payload + mlen + 1, tlv, tlv_len);

    int r = g_transport->send(g_transport->user,
                              (const uint8_t *)msg, total);
    free(msg);
    return r;
}

/* Request handling: fully table-driven */

static void rpc_handle_request(const struct rpc_message *msg) {
    const char *method = (const char *)msg->payload;

    size_t mlen = strlen(method);
    const uint8_t *tlv = msg->payload + mlen + 1;
    size_t tlv_len = msg->hdr.reserved;

    struct rpc_method_entry *m =
        hashmap_get(&g_methods, (void *)method);

    if (!m) {
        static const uint8_t err[] =
            { 0x02, 12, 0, 'N','O','T','_','F','O','U','N','D' };
        rpc_send_response_tlv(msg->hdr.req_id, err, sizeof(err));
        return;
    }

    /* parse_param → param struct (stack buffer, method-specific layout) */
    uint8_t param_buf[256];
    memset(param_buf, 0, sizeof(param_buf));
    void *param = param_buf;

    if (m->parse_param)
        m->parse_param(tlv, tlv_len, param);

    /* handler(param) → rpc_result */
    struct rpc_result r;
    memset(&r, 0, sizeof(r));

    m->handler(param, &r);

    /* rpc_result → TLV */
    uint8_t *resp_tlv = NULL;
    size_t   resp_len = 0;

    m->encode_result(&r, &resp_tlv, &resp_len);

    rpc_send_response_tlv(msg->hdr.req_id, resp_tlv, resp_len);

    free(resp_tlv);
}

/* Response handling: single-flight sync call */

static void rpc_handle_response(const struct rpc_message *msg) {
    if (msg->hdr.req_id != g_waiting_req_id)
        return;

    size_t len = msg->hdr.reserved;
    if (len > sizeof(g_response_buffer))
        len = sizeof(g_response_buffer);

    memcpy(g_response_buffer, msg->payload, len);
    g_response_len   = len;
    g_response_ready = 1;
}

/* Public API */

void rpc_init(void) {
    hashmap_init(&g_methods, 64, HASHMAP_KEY_STRING);
    g_transport       = NULL;
    g_next_req_id     = 1;
    g_waiting_req_id  = 0;
    g_response_ready  = 0;
    g_response_len    = 0;
}

void rpc_set_transport(rpc_transport_t *t) {
    g_transport = t;
}

int rpc_call_with_tlv(const char *method,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *resp_tlv, size_t *resp_len)
{
    uint16_t req_id = rpc_next_req_id();
    g_waiting_req_id  = req_id;
    g_response_ready  = 0;

    int r = rpc_send_request_tlv(req_id, method, tlv, tlv_len);
    if (r < 0)
        return r;

    /* simple sync wait; transport is expected to drive rpc_poll() */
    while (!g_response_ready)
        rpc_poll();

    if (resp_tlv && resp_len) {
        size_t len = g_response_len;
        memcpy(resp_tlv, g_response_buffer, len);
        *resp_len = len;
    }

    return 0;
}

void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder)
{
    struct rpc_method_entry *e = malloc(sizeof(*e));
    if (!e)
        return;

    e->name         = name;
    e->parse_param  = parser;
    e->handler      = handler;
    e->encode_result = encoder;

    hashmap_put(&g_methods, (void *)name, e);
}

void rpc_poll(void) {
    if (!g_transport)
        return;

    uint8_t buf[512];
    ssize_t n = g_transport->recv(g_transport->user, buf, sizeof(buf));
    if (n <= 0)
        return;

    struct rpc_message *msg = rpc_decode(buf, (size_t)n);
    if (!msg)
        return;

    if (msg->hdr.type == RPC_MSG_REQUEST)
        rpc_handle_request(msg);
    else if (msg->hdr.type == RPC_MSG_RESPONSE)
        rpc_handle_response(msg);

    free(msg);
}
