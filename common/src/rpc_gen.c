#include "rpc_gen.h"
#include <string.h>
#include <stdlib.h>

/* 为了 stub/encoder 使用的最大 TLV 尺寸 */
#ifndef RPC_MAX_WIRE_TLV_SIZE
#define RPC_MAX_WIRE_TLV_SIZE 512
#endif

/* ============================================
 * Handler forward declare（仅 PROVIDER）
 * ============================================ */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int name##_handler(const struct rpc_param_##name *in,           \
                       struct rpc_result_##name *out);

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    /* requester 不提供 handler */

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

/* ============================================
 * TLV 读写辅助：解析一个字段（顺序读取）
 * ============================================ */

/* 解析 string/u32/i32/bytes 到指定字段，并推进 p/remain */

#define PARSE_FIELD_string(fieldname)                                   \
    do {                                                                \
        char *s = NULL;                                                 \
        size_t used = tlv_read_string(p, remain, &s);                   \
        if (used == 0) return -1;                                       \
        (fieldname) = s;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)

#define PARSE_FIELD_u32(fieldname)                                      \
    do {                                                                \
        uint32_t v = 0;                                                 \
        size_t used = tlv_read_u32(p, remain, &v);                      \
        if (used == 0) return -1;                                       \
        (fieldname) = v;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)

#define PARSE_FIELD_i32(fieldname)                                      \
    do {                                                                \
        int32_t v = 0;                                                  \
        size_t used = tlv_read_i32(p, remain, &v);                      \
        if (used == 0) return -1;                                       \
        (fieldname) = v;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)

#define PARSE_FIELD_bytes(fieldname)                                    \
    do {                                                                \
        uint8_t *ptr = NULL;                                            \
        size_t   len = 0;                                               \
        size_t used = tlv_read_bytes(p, remain, &ptr, &len);            \
        if (used == 0) return -1;                                       \
        (fieldname).ptr = ptr;                                          \
        (fieldname).len = len;                                          \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)

/* 写入一个字段到 TLV buffer，并推进 off */

#define EMIT_FIELD_string(buf, off, value)                              \
    do {                                                                \
        if ((value))                                                   \
            (off) += tlv_write_string(&(buf)[off], (value));            \
    } while (0)

#define EMIT_FIELD_u32(buf, off, value)                                 \
    do {                                                                \
        (off) += tlv_write_u32(&(buf)[off], (value));                   \
    } while (0)

#define EMIT_FIELD_i32(buf, off, value)                                 \
    do {                                                                \
        (off) += tlv_write_i32(&(buf)[off], (value));                   \
    } while (0)

#define EMIT_FIELD_bytes(buf, off, value)                               \
    do {                                                                \
        if ((value).ptr && (value).len)                                 \
            (off) += tlv_write_bytes(&(buf)[off], (value).ptr, (value).len); \
    } while (0)

/* ============================================
 * param parser（PROVIDER + REQUEST）
 * ============================================ */

#define FIELD(type, name) PARSE_FIELD_##type(out->name)
#define PARAMS(...) __VA_ARGS__

#define GEN_PARAM_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)        \
    int rpc_param_parse_##name(const uint8_t *tlv, size_t len,          \
                               struct rpc_param_##name *out)            \
    {                                                                   \
        const uint8_t *p = tlv;                                         \
        size_t remain = len;                                            \
        memset(out, 0, sizeof(*out));                                   \
        do { PARAM_LIST } while (0);                                    \
        return 0;                                                       \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    GEN_PARAM_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    GEN_PARAM_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_PARAM_PARSER
#undef FIELD
#undef PARAMS

/* ============================================
 * result parser（PROVIDER + REQUEST）
 *  - 客户端解析响应 TLV → result struct
 * ============================================ */

#define FIELD(type, name) PARSE_FIELD_##type(out->name)
#define RESULTS(...) __VA_ARGS__

#define GEN_RESULT_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)       \
    int rpc_result_parse_##name(const uint8_t *tlv, size_t len,         \
                                struct rpc_result_##name *out)          \
    {                                                                   \
        const uint8_t *p = tlv;                                         \
        size_t remain = len;                                            \
        memset(out, 0, sizeof(*out));                                   \
        do { RESULT_LIST } while (0);                                   \
        return 0;                                                       \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    GEN_RESULT_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    GEN_RESULT_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_RESULT_PARSER
#undef FIELD
#undef RESULTS

/* ============================================
 * stub（PROVIDER + REQUEST 都生成 rpc_call_xxx）
 * ============================================ */

#define FIELD(type, name) EMIT_FIELD_##type(tlvbuf, off, in->name)
#define PARAMS(...) __VA_ARGS__

#define GEN_STUB(name, rpcname, PARAM_LIST, RESULT_LIST)                \
    int rpc_call_##name(const struct rpc_param_##name *in,              \
                        struct rpc_result_##name *out)                  \
    {                                                                   \
        uint8_t tlvbuf[RPC_MAX_WIRE_TLV_SIZE];                          \
        size_t  off = 0;                                                \
        memset(tlvbuf, 0, sizeof(tlvbuf));                              \
        do { PARAM_LIST } while (0);                                    \
                                                                        \
        uint8_t resp[RPC_MAX_WIRE_TLV_SIZE];                            \
        size_t  resp_len = sizeof(resp);                                \
        memset(resp, 0, sizeof(resp));                                  \
                                                                        \
        int st = rpc_call_with_tlv(rpcname, tlvbuf, off,                \
                                   resp, &resp_len);                    \
        if (st != 0)                                                    \
            return st;                                                  \
                                                                        \
        return rpc_result_parse_##name(resp, resp_len, out);            \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    GEN_STUB(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    GEN_STUB(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_STUB
#undef FIELD
#undef PARAMS

/* ============================================
 * per-method result encoder（仅 PROVIDER）
 *  - result struct → TLV（字段顺序同 RESULTS）
 * ============================================ */

#define FIELD(type, name) EMIT_FIELD_##type(buf, off, r->name)
#define RESULTS(...) __VA_ARGS__

#define GEN_RESULT_ENCODER(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    static int encode_result_##name(void *result_struct,                \
                                    uint8_t *buf, size_t *out_len)      \
    {                                                                   \
        struct rpc_result_##name *r =                                   \
            (struct rpc_result_##name *)result_struct;                  \
        size_t off = 0;                                                 \
        if (!buf || !out_len)                                           \
            return -1;                                                  \
        memset(buf, 0, RPC_MAX_WIRE_TLV_SIZE);                          \
        do { RESULT_LIST } while (0);                                   \
        *out_len = off;                                                 \
        return 0;                                                       \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    GEN_RESULT_ENCODER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    /* requester 不需要 encoder */

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_RESULT_ENCODER
#undef FIELD
#undef RESULTS

/* ============================================
 * register_xxx（仅 PROVIDER）
 * ============================================ */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    void rpc_register_##name(void)                                      \
    {                                                                   \
        rpc_register_method(                                            \
            rpcname,                                                    \
            (rpc_param_parser_t)rpc_param_parse_##name,                 \
            (rpc_handler_t)name##_handler,                              \
            (rpc_result_encoder_t)encode_result_##name                  \
        );                                                              \
    }

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    /* requester 不注册 handler */

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

/* ============================================
 * rpc_register_all：只注册 PROVIDER 方法
 * ============================================ */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    rpc_register_##name();

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    /* requester 不参与 register_all */

void rpc_register_all(void)
{
#include RPC_METHODS_XDEF_FILE
}

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
