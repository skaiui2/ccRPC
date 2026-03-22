#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpc_gen.h"
#include "heap.h"

int shell_exec_handler(const struct rpc_param_shell_exec *in,
                       struct rpc_result_shell_exec *out)
{
    FILE *fp = popen(in->cmd, "r");
    if (!fp)
        return -1;

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf)-1, fp);
    buf[n] = 0;

    int exitcode = pclose(fp);

    char *s = heap_malloc(n + 1);
    if (!s)
        return -1;

    memcpy(s, buf, n + 1);

    out->output   = s;
    out->exitcode = (uint32_t)exitcode;

    return 0;
}
