#pragma once

#include <stdint.h>
#include <stddef.h>
#include "rpc_gen.h"  


int fs_read_handler(const struct rpc_param_fs_read *in,
                    struct rpc_result_fs_read *out);