#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* Message types */
#define RPC_MSG_REQUEST   0
#define RPC_MSG_RESPONSE  1

/* Wire header (packed) */
struct rpc_header {
    uint16_t req_id;
    uint8_t  type;
    uint8_t  reserved;   /* TLV length (0–255) */
};

/* Full message: header + payload */
struct rpc_message {
    struct rpc_header hdr;
    uint8_t payload[];
};

/* Transport abstraction */
typedef struct {
    ssize_t (*send)(void *user, const uint8_t *buf, size_t len);
    ssize_t (*recv)(void *user, uint8_t *buf, size_t maxlen);
    void *user;
} rpc_transport_t;

/* Forward declarations for method registry types */
struct rpc_method_entry;
typedef int (*rpc_param_parser_t)(
    const uint8_t *tlv, size_t tlv_len,
    void *param_out);
struct rpc_result;
typedef int (*rpc_handler_t)(
    const void *param,
    struct rpc_result *result);
typedef int (*rpc_result_encoder_t)(
    const struct rpc_result *r,
    uint8_t **out, size_t *out_len);

/* Core API */
void rpc_init(void);
void rpc_set_transport(rpc_transport_t *t);

int rpc_call_with_tlv(const char *method,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *resp_tlv, size_t *resp_len);

void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder);

void rpc_poll(void);
