#include "rpc_gen.h"
#include "heap.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    int name##_handler(const struct rpc_param_##name *in,           \
                       struct rpc_result_##name *out);

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

/* ---------- param parsers: TLV -> rpc_param_xxx ---------- */

#ifndef PARSE_FIELD_string
#define PARSE_FIELD_string(fieldname)                                   \
    do {                                                                \
        char *s = NULL;                                                 \
        size_t used = tlv_read_string(p, remain, &s);                   \
        if (used == 0) return -1;                                       \
        (fieldname) = s;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)
#endif

#ifndef PARSE_FIELD_u32
#define PARSE_FIELD_u32(fieldname)                                      \
    do {                                                                \
        uint32_t v = 0;                                                 \
        size_t used = tlv_read_u32(p, remain, &v);                      \
        if (used == 0) return -1;                                       \
        (fieldname) = v;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)
#endif

#ifndef PARSE_FIELD_i32
#define PARSE_FIELD_i32(fieldname)                                      \
    do {                                                                \
        int32_t v = 0;                                                  \
        size_t used = tlv_read_i32(p, remain, &v);                      \
        if (used == 0) return -1;                                       \
        (fieldname) = v;                                                \
        p      += used;                                                 \
        remain -= used;                                                 \
    } while (0)
#endif

#ifndef PARSE_FIELD_bytes
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
#endif

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

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_PARAM_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_PARAM_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_PARAM_PARSER
#undef FIELD
#undef PARAMS

/* ---------- result parsers: TLV -> rpc_result_xxx ---------- */

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

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_RESULT_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_RESULT_PARSER(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_RESULT_PARSER
#undef FIELD
#undef RESULTS

/* ---------- emit helpers for stub and result encoder ---------- */

#ifndef RPC_EMIT_FIELD_string
#define RPC_EMIT_FIELD_string(buf, off, value)                          \
    do {                                                                \
        if (value)                                                      \
            off += tlv_write_string(&(buf)[off], value);                \
    } while (0)
#endif

#ifndef RPC_EMIT_FIELD_u32
#define RPC_EMIT_FIELD_u32(buf, off, value)                             \
    do {                                                                \
        off += tlv_write_u32(&(buf)[off], value);                       \
    } while (0)
#endif

#ifndef RPC_EMIT_FIELD_i32
#define RPC_EMIT_FIELD_i32(buf, off, value)                             \
    do {                                                                \
        off += tlv_write_i32(&(buf)[off], value);                       \
    } while (0)
#endif

#ifndef RPC_EMIT_FIELD_bytes
#define RPC_EMIT_FIELD_bytes(buf, off, value)                           \
    do {                                                                \
        off += tlv_write_bytes(&(buf)[off], (value).ptr, (value).len);  \
    } while (0)
#endif

static size_t rpc_estimate_field_string(const char *s)
{
    size_t len = s ? strlen(s) : 0;
    return 1 + 2 + len;
}

static size_t rpc_estimate_field_u32(uint32_t v)
{
    (void)v;
    return 1+2+4;
}

static size_t rpc_estimate_field_i32(int32_t v)
{
    (void)v;
    return 1+2+4;
}

static size_t rpc_estimate_field_bytes(rpc_bytes_t b)
{
    return 1+2+b.len;
}

#define FIELD(type, name) sz += rpc_estimate_field_##type(in->name)
#define PARAMS(...) __VA_ARGS__
#define GEN_ESTIMATE(name, rpcname, PARAM_LIST, RESULT_LIST) \
    static size_t rpc_estimate_param_size_##name(const struct rpc_param_##name *in) { \
        size_t sz = 0; do { PARAM_LIST } while (0); return sz; }
#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)
#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_ESTIMATE(name, rpcname, PARAM_LIST, RESULT_LIST)
#include RPC_METHODS_XDEF_FILE
#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_ESTIMATE
#undef FIELD
#undef PARAMS

/* ---------- stubs: rpc_call_xxx (heap-based buffers) ---------- */
#define FIELD(type, name) RPC_EMIT_FIELD_##type(tlvbuf, off, in->name)
#define PARAMS(...) __VA_ARGS__

#define GEN_STUB(name, rpcname, PARAM_LIST, RESULT_LIST)                \
    int rpc_call_##name(const struct rpc_param_##name *in,              \
                        struct rpc_result_##name *out,                  \
                        uint32_t timeout_ms)                            \
    {                                                                   \
        size_t tlv_size = rpc_estimate_param_size_##name(in);           \
        uint8_t *tlvbuf = (uint8_t *)RPC_ALLOC(tlv_size);               \
        if (!tlvbuf)                                                    \
            return -1;                                                  \
        memset(tlvbuf, 0, tlv_size);                                    \
        size_t off = 0;                                                 \
        do { PARAM_LIST } while (0);                                    \
                                                                        \
        size_t resp_len = RPC_RESP_BUF_SIZE;                            \
        uint8_t *resp = (uint8_t *)RPC_ALLOC(resp_len);                 \
        if (!resp) {                                                    \
            RPC_FREE(tlvbuf);                                           \
            return -1;                                                  \
        }                                                               \
        memset(resp, 0, resp_len);                                      \
                                                                        \
        int st = rpc_call_with_tlv(rpcname, tlvbuf, off, resp, &resp_len, timeout_ms); \
        if (st == 0)                                                    \
            st = rpc_result_parse_##name(resp, resp_len, out);          \
                                                                        \
        RPC_FREE(tlvbuf);                                               \
        RPC_FREE(resp);                                                 \
        return st;                                                      \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_STUB(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_STUB
#undef FIELD
#undef PARAMS

/* ---------- result encoders: rpc_result_xxx -> TLV ---------- */

#define FIELD(type, name) RPC_EMIT_FIELD_##type(buf, off, r->name)
#define RESULTS(...) __VA_ARGS__

#define GEN_RESULT_ENCODER(name, rpcname, PARAM_LIST, RESULT_LIST)      \
    static int encode_result_##name(void *result_struct,                \
                                    uint8_t *buf, size_t *out_len)      \
    {                                                                   \
        struct rpc_result_##name *r = result_struct;                    \
        size_t off = 0;                                                 \
        if (out_len && *out_len > 0)                                    \
            memset(buf, 0, *out_len);                                   \
        do { RESULT_LIST } while (0);                                   \
        *out_len = off;                                                 \
        return 0;                                                       \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_RESULT_ENCODER(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_RESULT_ENCODER
#undef FIELD
#undef RESULTS

/* ---------- free_param_xxx: free heap fields in param structs ---------- */

#define FIELD(type, name) FREE_FIELD_##type(p->name)
#define PARAMS(...) __VA_ARGS__

#ifndef FREE_FIELD_string
#define FREE_FIELD_string(fieldname) \
    do { if (fieldname) RPC_FREE(fieldname); } while (0)
#endif

#ifndef FREE_FIELD_u32
#define FREE_FIELD_u32(fieldname) \
    do { } while (0)
#endif

#ifndef FREE_FIELD_i32
#define FREE_FIELD_i32(fieldname) \
    do { } while (0)
#endif

#ifndef FREE_FIELD_bytes
#define FREE_FIELD_bytes(fieldname) \
    do { if ((fieldname).ptr) RPC_FREE((fieldname).ptr); } while (0)
#endif

#define GEN_FREE_PARAM(name, rpcname, PARAM_LIST, RESULT_LIST) \
    void free_param_##name(struct rpc_param_##name *p)         \
    {                                                          \
        do { PARAM_LIST } while (0);                           \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_FREE_PARAM(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_FREE_PARAM(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_FREE_PARAM
#undef FIELD
#undef PARAMS

/* ---------- free_result_xxx: free heap fields in result structs ---------- */

#define FIELD(type, name) FREE_FIELD_##type(r->name)
#define RESULTS(...) __VA_ARGS__

#define GEN_FREE_RESULT(name, rpcname, PARAM_LIST, RESULT_LIST) \
    void free_result_##name(struct rpc_result_##name *r)        \
    {                                                           \
        do { RESULT_LIST } while (0);                           \
    }

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_FREE_RESULT(name, rpcname, PARAM_LIST, RESULT_LIST)

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST) \
    GEN_FREE_RESULT(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
#undef GEN_FREE_RESULT
#undef FIELD
#undef RESULTS

/* ---------- per-method register functions ---------- */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST)     \
    void rpc_register_##name(void)                                      \
    {                                                                   \
        rpc_register_method(                                            \
            rpcname,                                                    \
            (rpc_param_parser_t)rpc_param_parse_##name,                 \
            (rpc_handler_t)name##_handler,                              \
            (rpc_result_encoder_t)encode_result_##name,                 \
            (rpc_free_param_t)free_param_##name,                        \
            (rpc_free_param_t)free_result_##name                        \
        );                                                              \
    }

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST

/* ---------- register all ---------- */

#define RPC_METHOD_PROVIDER(name, rpcname, PARAM_LIST, RESULT_LIST) \
    rpc_register_##name();

#define RPC_METHOD_REQUEST(name, rpcname, PARAM_LIST, RESULT_LIST)

void rpc_register_all(void)
{
#include RPC_METHODS_XDEF_FILE
}

#undef RPC_METHOD_PROVIDER
#undef RPC_METHOD_REQUEST
