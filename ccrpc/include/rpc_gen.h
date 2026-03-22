#ifndef RPC_GEN_H
#define RPC_GEN_H

#include <stdint.h>
#include <stddef.h>
#include "rpc_tlv.h"
#include "rpc.h"
#include "heap.h"

#define RPC_ALLOC(sz)        heap_malloc(sz)
#define RPC_FREE(p)          heap_free(p)
#define RPC_STRING_ALLOC(len) heap_malloc(len)
#define RPC_BYTES_ALLOC(len)  heap_malloc(len)

#define RPC_RESP_BUF_SIZE    4096

typedef char*    rpc_string_t;
typedef uint32_t rpc_u32_t;
typedef int32_t  rpc_i32_t;

typedef struct {
    uint8_t *ptr;
    size_t   len;
} rpc_bytes_t;

#define FIELD(type, name) rpc_##type##_t name;
#define PARAMS(...)  __VA_ARGS__
#define RESULTS(...) __VA_ARGS__

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    struct rpc_param_##name { PARAM_LIST };                         \
    struct rpc_result_##name { RESULT_LIST };

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    struct rpc_param_##name { PARAM_LIST };                        \
    struct rpc_result_##name { RESULT_LIST };

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef FIELD
#undef PARAMS
#undef RESULTS

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int  rpc_param_parse_##name(const uint8_t *tlv, size_t len, struct rpc_param_##name *out); \
    int  rpc_result_parse_##name(const uint8_t *tlv, size_t len, struct rpc_result_##name *out); \
    void rpc_register_##name(void); \
    void free_param_##name(struct rpc_param_##name *p); \
    void free_result_##name(struct rpc_result_##name *r);

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int  rpc_param_parse_##name(const uint8_t *tlv, size_t len, struct rpc_param_##name *out); \
    int  rpc_result_parse_##name(const uint8_t *tlv, size_t len, struct rpc_result_##name *out); \
    int  rpc_call_##name(const struct rpc_param_##name *in, struct rpc_result_##name *out, uint32_t timeout_ms); \
    void free_param_##name(struct rpc_param_##name *p); \
    void free_result_##name(struct rpc_result_##name *r);

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

void rpc_register_all(void);

#endif
