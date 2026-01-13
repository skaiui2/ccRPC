#ifndef RPC_TLV_H
#define RPC_TLV_H

#include <stdint.h>
#include <stddef.h>

struct rpc_tlv {
    uint8_t type;
    uint16_t len;
    uint8_t value[];
};

size_t tlv_write_u32(uint8_t *buf, uint32_t v);
size_t tlv_write_string(uint8_t *buf, const char *s);

size_t tlv_write_bytes(uint8_t *buf, const uint8_t *data, size_t len);

size_t tlv_read_string(const uint8_t *buf, size_t len, char **out);
size_t tlv_read_u32(const uint8_t *buf, size_t len, uint32_t *out);
size_t tlv_read_i32(const uint8_t *buf, size_t len, int32_t *out);
size_t tlv_read_bytes(const uint8_t *buf, size_t len, uint8_t **ptr, size_t *out_len);

#endif
