#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rpc_tlv.h"
#include "rpc_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include "services_shell.h"


int shell_exec_handler(const struct rpc_param_shell_exec *in,
                       struct rpc_result_shell_exec *out)
{
    printf("NodeB: shell_exec(cmd=%s)\n",
           in->cmd ? in->cmd : "(null)");

    out->output = "dummy-output-from-NodeB";
    out->exitcode = 0;

    return 0;
}
