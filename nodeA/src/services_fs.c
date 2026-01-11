#include "services_fs.h"
#include "rpc_tlv.h"    
#include <stdio.h>
#include <stdlib.h>
#include "rpc_tlv.h"
#include "rpc_gen.h"

#include "rpc_gen.h"
#include <stdio.h>
#include <string.h>

// 由 rpc_gen.c 自动 forward declare
int fs_read_handler(const struct rpc_param_fs_read *in,
                    struct rpc_result_fs_read *out)
{
    printf("NodeA: fs_read(path=%s, offset=%u, size=%u)\n",
           in->path ? in->path : "(null)",
           (unsigned)in->offset,
           (unsigned)in->size);

    //返回固定内容 
    static uint8_t dummy[] = "hello-from-NodeA";
    out->data.ptr = dummy;
    out->data.len = sizeof(dummy) - 1;
    out->len = out->data.len;
    out->eof = 1;

    return 0;
}
