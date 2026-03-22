#include "services_fs.h"
#include "rpc_tlv.h"    
#include <stdio.h>
#include <stdlib.h>
#include "rpc_tlv.h"
#include "rpc_gen.h"
#include "heap.h"
#include "rpc_gen.h"
#include <stdio.h>
#include <string.h>

int fs_read_handler(const struct rpc_param_fs_read *in,
                    struct rpc_result_fs_read *out)
{
    FILE *fp = fopen(in->path, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, in->offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    uint8_t *p = heap_malloc(in->size);
    if (!p) {
        fclose(fp);
        return -1;
    }

    size_t n = fread(p, 1, in->size, fp);
    fclose(fp);

    if (n != in->size) {
        heap_free(p);
        return -1;
    }

    out->data.ptr = p;
    out->data.len = n;
    out->len      = n;
    out->eof      = 0;
    return 0;
}
