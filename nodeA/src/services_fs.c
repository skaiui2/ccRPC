#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include "rpc_methods_decl.h"

int fs_read_handler(const struct rpc_param_fs_read *p,
                    struct rpc_result *r)
{
    printf("[NodeA] fs.read path=%s offset=%u size=%u\n",
           p->path, p->offset, p->size);

    uint8_t *buf = malloc(p->size);
    memset(buf, 'A', p->size);

    r->type = RPC_RESULT_BYTES;
    r->data = buf;
    r->len  = p->size;

    return 0;
}
