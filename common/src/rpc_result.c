#include "rpc_result.h"
#include <string.h>
#include <stdlib.h>

int rpc_result_to_tlv(const struct rpc_result *r,
                      uint8_t **out, size_t *out_len)
{
    size_t len = r->len;
    *out = malloc(3 + len);

    (*out)[0] = (r->type == RPC_RESULT_STRING) ? 0x02 : 0x03;
    (*out)[1] = len & 0xFF;
    (*out)[2] = (len >> 8) & 0xFF;

    memcpy(&(*out)[3], r->data, len);

    *out_len = 3 + len;
    return 0;
}
