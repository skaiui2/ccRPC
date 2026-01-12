#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define RPC_MSG_REQUEST   0
#define RPC_MSG_RESPONSE  1

#define RPC_VERSION_2_0   0x0200

#define RPC_FLAG_RESPONSE 0x0001
#define RPC_FLAG_ERROR    0x0002

typedef enum {
    RPC_STATUS_OK = 0,
    RPC_STATUS_METHOD_NOT_FOUND = 1,
    RPC_STATUS_INVALID_PARAMS   = 2,
    RPC_STATUS_INTERNAL_ERROR   = 3,
    RPC_STATUS_TRANSPORT_ERROR  = 4,
} rpc_status_t;

//all is net(big) endian
struct rpc_header {
    uint16_t version;    
    uint16_t flags;     
    uint32_t seq;      
    uint16_t method_len; 
    uint16_t status;   
};

struct rpc_message {
    struct rpc_header hdr;
    uint8_t payload[];
};

typedef struct {
    ssize_t (*send)(void *user, const uint8_t *buf, size_t len);
    ssize_t (*recv)(void *user, uint8_t *buf, size_t maxlen);
    void   (*close)(void *user);
    void *user;
} rpc_transport_t;

// TLV to param_struct 
typedef int (*rpc_param_parser_t)(
    const uint8_t *tlv, size_t tlv_len,
    void *param_out);

// param_struct to result_struct
typedef int (*rpc_handler_t)(
    const void *param,
    void *result);

// result_struct to TLV（
typedef int (*rpc_result_encoder_t)(
    void *result_struct,
    uint8_t *out_buf,
    size_t *out_len);

void rpc_init(void);
void rpc_set_transport(rpc_transport_t *t);
int  rpc_register_transport(rpc_transport_t *t);

void rpc_transport_register(const char *name, rpc_transport_t *t);
rpc_transport_t *rpc_transport_lookup(const char *name);

//requester call it
int rpc_call_with_tlv(const char *method,
                      const uint8_t *tlv, size_t tlv_len,
                      uint8_t *resp_tlv, size_t *resp_len);

//provider register it
void rpc_register_method(const char *name,
                         rpc_param_parser_t parser,
                         rpc_handler_t handler,
                         rpc_result_encoder_t encoder);

void rpc_poll(void);
