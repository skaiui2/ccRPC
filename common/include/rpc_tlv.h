#pragma once
#include <stdint.h>
#include <stddef.h>

struct rpc_tlv {
    uint8_t type;
    uint16_t len;
    uint8_t value[];
};

size_t tlv_write_u32(uint8_t *buf, uint32_t v);
size_t tlv_write_string(uint8_t *buf, const char *s);

size_t tlv_read(const uint8_t *buf, size_t buf_len,
                uint8_t *type, const uint8_t **value, uint16_t *len);
