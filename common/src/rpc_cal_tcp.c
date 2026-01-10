#include "rpc_cal_tcp.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int rpc_tcp_connect(rpc_tcp_ctx_t *ctx, const char *ip, int port) {
    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    return connect(ctx->sockfd, (struct sockaddr*)&addr, sizeof(addr));
}

int rpc_tcp_listen(rpc_tcp_ctx_t *ctx, int port) {
    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(ctx->sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(ctx->sockfd, 1);
    return 0;
}

int rpc_tcp_accept(rpc_tcp_ctx_t *listener, rpc_tcp_ctx_t *client) {
    client->sockfd = accept(listener->sockfd, NULL, NULL);
    return client->sockfd;
}

ssize_t rpc_tcp_send(void *user, const uint8_t *buf, size_t len) {
    rpc_tcp_ctx_t *ctx = (rpc_tcp_ctx_t*)user;
    return send(ctx->sockfd, buf, len, 0);
}

ssize_t rpc_tcp_recv(void *user, uint8_t *buf, size_t maxlen) {
    rpc_tcp_ctx_t *ctx = (rpc_tcp_ctx_t*)user;
    return recv(ctx->sockfd, buf, maxlen, 0);
}
