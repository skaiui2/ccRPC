#ifndef _RPC_H
#define _RPC_H

#include <stdint.h>
#include <stddef.h>
#include "link_list.h"

typedef enum {
    RPC_STATUS_OK = 0,
    RPC_STATUS_METHOD_NOT_FOUND = 1,
    RPC_STATUS_INVALID_PARAMS   = 2,
    RPC_STATUS_INTERNAL_ERROR   = 3,
    RPC_STATUS_TRANSPORT_ERROR  = 4,
    RPC_STATUS_TIMEOUT          = 5,
} rpc_status_t;

#define MTU 4096

struct rpc_buf {
    struct list_node node;
    size_t len;
    uint8_t data[];
};

struct rpc_reasm {
    struct list_node head;
    size_t total_len;
};

// all is net(big) endian
struct rpc_header {
    uint16_t status;
    uint32_t seq;
    uint32_t msg_len;    /* payload length: method + tlv */
    uint16_t method_len; /* length of method name in payload (request), 0 for response */
} __attribute__((packed));

struct rpc_message {
    struct rpc_header hdr;
    uint8_t payload[];
};

struct rpc_transport_class {
    size_t (*send)(void *user, const uint8_t *buf, size_t len);
    size_t (*recv)(void *user, uint8_t *buf, size_t maxlen);
    void   (*close)(void *user);
    void *user;
    struct rpc_reasm reasm;
};

// TLV to param_struct
typedef int (*rpc_param_parser_t)(
        const uint8_t *tlv, size_t tlv_len,
        void *param_out);

// param_struct to result_struct
typedef int (*rpc_handler_t)(
        const void *param,
        void *result);

// result_struct to TLV
typedef int (*rpc_result_encoder_t)(
        void *result_struct,
        uint8_t *out_buf,
        size_t *out_len);

typedef void (*rpc_free_param_t)(void *param);


int rpc_call_with_tlv(const char *name,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *out_tlv, size_t *out_len,
                      uint32_t timeout_ms);

void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder,
                         rpc_free_param_t free_param,
                         rpc_free_param_t free_result);

/* API */
void rpc_init(uint8_t method_cap, uint8_t pending_cap, uint8_t transport_cap);
struct rpc_transport_class *rpc_trans_class_create(void *send, void *recv, void *close, void *user);
void rpc_bind_transport(const char *name, struct rpc_transport_class *t);
struct rpc_transport_class *rpc_transport_lookup(const char *name);
void rpc_on_data(struct rpc_transport_class *t,
                 const uint8_t *buf, size_t len);

#endif
