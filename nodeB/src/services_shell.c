#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rpc_methods_decl.h"

int shell_exec_handler(const void *param, struct rpc_result *r)
{
    const struct rpc_param_shell_exec *p = param;

    printf("[NodeB] shell.exec cmd=%s\n",
           p->cmd ? p->cmd : "(null)");

    static const char msg[] = "OK";
    r->type = RPC_RESULT_STRING;
    r->data = msg;
    r->len  = sizeof(msg) - 1;

    return 0;
}