#include "rpc_tlv.h"
#include <string.h>
#include <stdlib.h>

size_t tlv_write_u32(uint8_t *buf, uint32_t v) {
    buf[0] = 0x01;
    buf[1] = 4;
    buf[2] = 0;
    memcpy(&buf[3], &v, 4);
    return 1 + 2 + 4;
}

size_t tlv_write_bytes(uint8_t *buf, const uint8_t *data, size_t len)
{
    buf[0] = 0x03;              // bytes 类型
    buf[1] = len & 0xFF;
    buf[2] = (len >> 8) & 0xFF;
    memcpy(&buf[3], data, len);
    return 1 + 2 + len;
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

size_t tlv_read_string(const uint8_t *buf, size_t buf_len, char **out)
{
    uint8_t type;
    const uint8_t *value;
    uint16_t len;

    size_t used = tlv_read(buf, buf_len, &type, &value, &len);
    if (used == 0 || type != 0x02)
        return 0;

    char *s = malloc(len + 1);
    memcpy(s, value, len);
    s[len] = '\0';

    *out = s;
    return used;
}

size_t tlv_read_u32(const uint8_t *buf, size_t buf_len, uint32_t *out)
{
    uint8_t type;
    const uint8_t *value;
    uint16_t len;

    size_t used = tlv_read(buf, buf_len, &type, &value, &len);
    if (used == 0 || type != 0x01 || len != 4)
        return 0;

    memcpy(out, value, 4);
    return used;
}

size_t tlv_read_i32(const uint8_t *buf, size_t buf_len, int32_t *out)
{
    uint8_t type;
    const uint8_t *value;
    uint16_t len;

    size_t used = tlv_read(buf, buf_len, &type, &value, &len);
    if (used == 0 || type != 0x01 || len != 4)
        return 0;

    memcpy(out, value, 4);
    return used;
}

size_t tlv_read_bytes(const uint8_t *buf, size_t buf_len,
                      uint8_t **ptr, size_t *out_len)
{
    uint8_t type;
    const uint8_t *value;
    uint16_t len;

    size_t used = tlv_read(buf, buf_len, &type, &value, &len);
    if (used == 0 || type != 0x03)
        return 0;

    uint8_t *p = malloc(len);
    memcpy(p, value, len);

    *ptr = p;
    *out_len = len;
    return used;
}