#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    int sockfd;
} rpc_tcp_ctx_t;

int rpc_tcp_connect(rpc_tcp_ctx_t *ctx, const char *ip, int port);
int rpc_tcp_listen(rpc_tcp_ctx_t *ctx, int port);
int rpc_tcp_accept(rpc_tcp_ctx_t *listener, rpc_tcp_ctx_t *client);

ssize_t rpc_tcp_send(void *user, const uint8_t *buf, size_t len);
ssize_t rpc_tcp_recv(void *user, uint8_t *buf, size_t maxlen);

void rpc_tcp_close(void *user);
