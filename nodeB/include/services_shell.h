#ifndef SERVICES_SHELL_H
#define SERVICES_SHELL_H

#include "rpc_gen.h"  

int shell_exec_handler(const struct rpc_param_shell_exec *in,
                       struct rpc_result_shell_exec *out);

#endif
