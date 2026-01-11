#include "rpc_cal_tcp.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "rpc_cal_tcp.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int rpc_tcp_connect(rpc_tcp_ctx_t *ctx, const char *ip, int port)
{
    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(ctx->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(ctx->sockfd);
        return -1;
    }

    return 0;
}

int rpc_tcp_listen(rpc_tcp_ctx_t *ctx, int port)
{
    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sockfd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(ctx->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(ctx->sockfd);
        return -1;
    }

    if (listen(ctx->sockfd, 1) < 0) {
        perror("listen");
        close(ctx->sockfd);
        return -1;
    }

    return 0;
}

int rpc_tcp_accept(rpc_tcp_ctx_t *listener, rpc_tcp_ctx_t *client)
{
    client->sockfd = accept(listener->sockfd, NULL, NULL);
    if (client->sockfd < 0) {
        perror("accept");
        return -1;
    }
    return 0;
}

ssize_t rpc_tcp_send(void *user, const uint8_t *buf, size_t len)
{
    rpc_tcp_ctx_t *ctx = (rpc_tcp_ctx_t*)user;
    return send(ctx->sockfd, buf, len, 0);
}

ssize_t rpc_tcp_recv(void *user, uint8_t *buf, size_t maxlen)
{
    rpc_tcp_ctx_t *ctx = (rpc_tcp_ctx_t*)user;
    return recv(ctx->sockfd, buf, maxlen, 0);
}
