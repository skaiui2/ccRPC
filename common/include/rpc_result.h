#pragma once
#include <stdint.h>
#include <stddef.h>

#define RPC_RESULT_STRING  1
#define RPC_RESULT_BYTES   2

struct rpc_result {
    uint8_t type;
    const void *data;
    size_t len;
};


int rpc_result_to_tlv(const struct rpc_result *r,
                      uint8_t **out, size_t *out_len);