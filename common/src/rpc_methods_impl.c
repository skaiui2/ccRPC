#include <stdlib.h>
#include <string.h>
#include "rpc.h"
#include "rpc_tlv.h"
#include "rpc_methods_decl.h"

#define TLV_TYPE_STRING  0x02
#define TLV_TYPE_UINT32  0x01
#define TLV_TYPE_BOOL    0x03

/* 初始化字段的宏（按 kind） */

#define RPC_INIT_FIELD_RPC_FIELD_STRING(p, fname)      (p)->fname = NULL;
#define RPC_INIT_FIELD_RPC_FIELD_INT32(p, fname)       (p)->fname = 0;
#define RPC_INIT_FIELD_RPC_FIELD_UINT32(p, fname)      (p)->fname = 0;
#define RPC_INIT_FIELD_RPC_FIELD_BOOL(p, fname)        (p)->fname = 0;
#define RPC_INIT_FIELD_RPC_FIELD_PTR_shell_ctx(p, fname) /* no-op */

/* 解析字段的宏（按 kind） */

#define RPC_PARSE_FIELD_RPC_FIELD_STRING(p, idx, fname, type, value, vlen) \
    case idx: {                                                            \
        if (type == TLV_TYPE_STRING) {                                     \
            char *buf = malloc(vlen + 1);                                  \
            if (buf) {                                                     \
                memcpy(buf, value, vlen);                                  \
                buf[vlen] = 0;                                             \
                (p)->fname = buf;                                          \
            }                                                              \
        }                                                                  \
    } break;

#define RPC_PARSE_FIELD_RPC_FIELD_INT32(p, idx, fname, type, value, vlen)  \
    case idx: {                                                            \
        if (type == TLV_TYPE_UINT32 && vlen == 4) {                        \
            int32_t tmp;                                                   \
            memcpy(&tmp, value, 4);                                        \
            (p)->fname = tmp;                                              \
        }                                                                  \
    } break;

#define RPC_PARSE_FIELD_RPC_FIELD_UINT32(p, idx, fname, type, value, vlen) \
    case idx: {                                                            \
        if (type == TLV_TYPE_UINT32 && vlen == 4) {                        \
            uint32_t tmp;                                                  \
            memcpy(&tmp, value, 4);                                        \
            (p)->fname = tmp;                                              \
        }                                                                  \
    } break;

#define RPC_PARSE_FIELD_RPC_FIELD_BOOL(p, idx, fname, type, value, vlen)   \
    case idx: {                                                            \
        if (type == TLV_TYPE_BOOL && vlen == 1) {                          \
            (p)->fname = value[0] ? 1 : 0;                                 \
        }                                                                  \
    } break;

#define RPC_PARSE_FIELD_RPC_FIELD_PTR_shell_ctx(p, idx, fname, type, value, vlen) \
    /* no-op */

/* ---------- PASS 2: 生成初始化函数 ---------- */

#ifdef RPC_METHOD
#undef RPC_METHOD
#endif

#define X(idx, fname, fkind) RPC_INIT_FIELD_##fkind(p, fname)
#define RPC_METHOD(name, rpcname, PARAM_LIST, RESULT_KIND)          \
    static void rpc_init_params_##name(struct rpc_param_##name *p)  \
    {                                                               \
        PARAM_LIST                                                  \
    }

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD
#undef X

/* ---------- PASS 3: 生成解析函数 ---------- */

#define X(idx, fname, fkind) \
    RPC_PARSE_FIELD_##fkind(p, idx, fname, type, value, vlen)

#define RPC_METHOD(name, rpcname, PARAM_LIST, RESULT_KIND)                  \
    int rpc_param_parse_##name(const uint8_t *tlv, size_t tlv_len, void *out) \
    {                                                                       \
        struct rpc_param_##name *p = out;                                   \
        rpc_init_params_##name(p);                                          \
        const uint8_t *ptr = tlv;                                           \
        size_t remain = tlv_len;                                            \
        int index = 0;                                                      \
        while (remain > 0) {                                                \
            uint8_t type;                                                   \
            const uint8_t *value;                                           \
            uint16_t vlen;                                                  \
            size_t used = tlv_read(ptr, remain, &type, &value, &vlen);      \
            if (!used)                                                      \
                break;                                                      \
            switch (index) {                                                \
                PARAM_LIST                                                  \
                default:                                                    \
                    break;                                                  \
            }                                                               \
            index++;                                                        \
            ptr    += used;                                                 \
            remain -= used;                                                 \
        }                                                                   \
        return 0;                                                           \
    }

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD
#undef X

/* ---------- PASS 4: 生成注册函数 ---------- */

#define RPC_METHOD(name, rpcname, PARAM_LIST, RESULT_KIND)                  \
    extern int name##_handler(const struct rpc_param_##name *p,             \
                              struct rpc_result *r);                        \
    static int name##_handler_trampoline(const void *param,                 \
                                         struct rpc_result *r)             \
    {                                                                       \
        return name##_handler((const struct rpc_param_##name *)param, r);   \
    }                                                                       \
    void rpc_register_##name(void)                                          \
    {                                                                       \
        rpc_register_method(rpcname,                                        \
                            rpc_param_parse_##name,                         \
                            name##_handler_trampoline,                      \
                            rpc_result_to_tlv);                             \
    }

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD
