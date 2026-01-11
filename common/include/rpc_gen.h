#pragma once

#include <stdint.h>
#include <stddef.h>
#include "rpc_tlv.h"
#include "rpc.h"

/* ============================================
 * IDL → C 类型映射
 * ============================================ */

typedef char*    rpc_string_t;
typedef uint32_t rpc_u32_t;
typedef int32_t  rpc_i32_t;

typedef struct {
    uint8_t *ptr;
    size_t   len;
} rpc_bytes_t;

/* ============================================
 * 生成 param/result struct
 *  - PROVIDER 和 REQUEST 都要
 * ============================================ */

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

/* ============================================
 * 生成 parser / stub / register 声明
 * ============================================ */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int rpc_param_parse_##name(const uint8_t *tlv, size_t len, struct rpc_param_##name *out); \
    int rpc_result_parse_##name(const uint8_t *tlv, size_t len, struct rpc_result_##name *out); \
    int rpc_call_##name(const struct rpc_param_##name *in, struct rpc_result_##name *out); \
    void rpc_register_##name(void);  /* provider 特有 */

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int rpc_param_parse_##name(const uint8_t *tlv, size_t len, struct rpc_param_##name *out); \
    int rpc_result_parse_##name(const uint8_t *tlv, size_t len, struct rpc_result_##name *out); \
    int rpc_call_##name(const struct rpc_param_##name *in, struct rpc_result_##name *out); \
    /* requester 不声明 rpc_register_xxx */

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

/* 只有 PROVIDER 方法会被纳入 rpc_register_all */
void rpc_register_all(void);
