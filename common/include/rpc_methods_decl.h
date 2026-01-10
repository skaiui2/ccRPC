#pragma once

#include <stdint.h>
#include <stddef.h>
#include "rpc_result.h"

/* 字段 kind 标识（xdef 里用的第三个参数） */
#define RPC_FIELD_STRING      RPC_FIELD_STRING
#define RPC_FIELD_INT32       RPC_FIELD_INT32
#define RPC_FIELD_UINT32      RPC_FIELD_UINT32
#define RPC_FIELD_BOOL        RPC_FIELD_BOOL
#define RPC_FIELD_PTR(type)   RPC_FIELD_PTR_##type

/* 字段 kind → C 结构体字段类型 */
#define RPC_STRUCT_FIELD_RPC_FIELD_STRING(fname)        const char *fname;
#define RPC_STRUCT_FIELD_RPC_FIELD_INT32(fname)         int32_t    fname;
#define RPC_STRUCT_FIELD_RPC_FIELD_UINT32(fname)        uint32_t   fname;
#define RPC_STRUCT_FIELD_RPC_FIELD_BOOL(fname)          uint8_t    fname;
#define RPC_STRUCT_FIELD_RPC_FIELD_PTR_shell_ctx(fname) struct shell_ctx *fname

/* ---------- PASS 1: 展开 xdef 生成 struct 定义 ---------- */

#ifdef RPC_METHOD
#undef RPC_METHOD
#endif

#define X(idx, fname, fkind) RPC_STRUCT_FIELD_##fkind(fname)
#define RPC_METHOD(name, rpcname, PARAM_LIST, RESULT_KIND) \
    struct rpc_param_##name { PARAM_LIST };

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD
#undef X

/* ---------- PASS 2: 生成解析/注册函数原型 ---------- */

#define RPC_METHOD(name, rpcname, PARAM_LIST, RESULT_KIND) \
    int  rpc_param_parse_##name(const uint8_t *tlv, size_t tlv_len, void *out); \
    void rpc_register_##name(void);

#include RPC_METHODS_XDEF_FILE

#undef RPC_METHOD
