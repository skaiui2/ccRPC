#include "rpc_tlv.h"
#include <string.h>

size_t tlv_write_u32(uint8_t *buf, uint32_t v) {
    buf[0] = 0x01;
    buf[1] = 4;
    buf[2] = 0;
    memcpy(&buf[3], &v, 4);
    return 1 + 2 + 4;
}

size_t tlv_write_string(uint8_t *buf, const char *s) {
    size_t len = strlen(s);
    buf[0] = 0x02;
    buf[1] = len & 0xFF;
    buf[2] = (len >> 8) & 0xFF;
    memcpy(&buf[3], s, len);
    return 1 + 2 + len;
}

size_t tlv_read(const uint8_t *buf, size_t buf_len,
                uint8_t *type, const uint8_t **value, uint16_t *len)
{
    if (buf_len < 3) return 0;

    *type = buf[0];
    *len = buf[1] | (buf[2] << 8);

    if (buf_len < 3 + *len) return 0;

    *value = &buf[3];
    return 3 + *len;
}
