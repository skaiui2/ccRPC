#pragma once

#include <stddef.h>
#include <stdint.h>

/* Forward declarations so this header不依赖 rpc.h */
struct rpc_result;

/* These typedefs要与 rpc.h 里的前向声明保持一致 */
typedef int (*rpc_param_parser_t)(
    const uint8_t *tlv, size_t tlv_len,
    void *param_out);

typedef int (*rpc_handler_t)(
    const void *param,
    struct rpc_result *result);

typedef int (*rpc_result_encoder_t)(
    const struct rpc_result *r,
    uint8_t **out, size_t *out_len);

struct rpc_method_entry {
    const char *name;
    rpc_param_parser_t    parse_param;
    rpc_handler_t         handler;
    rpc_result_encoder_t  encode_result;
};
