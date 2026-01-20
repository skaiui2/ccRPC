#ifndef CCNET_H
#define CCNET_H
#include <stdint.h>
#include <stddef.h>

#define CCNET_UPDATE_PERIOD 20
#define CCNET_TTL 255

#define CCNET_MAX_NODES 16
#define CCNET_INF 0x3fff

#define CCNET_BASE_WEIGHT 80   
#define CCNET_RTT_WEIGHT  20   

struct ccnet_dyn {
    int rtt[CCNET_MAX_NODES][CCNET_MAX_NODES];
};

struct ccnet_graph {
    int cost[CCNET_MAX_NODES][CCNET_MAX_NODES];
    int node_count;
};

struct ccnet_dijkstra {
    int dist[CCNET_MAX_NODES];
    int prev[CCNET_MAX_NODES];
    int visited[CCNET_MAX_NODES];
};



enum { 
    CCNET_TYPE_DATA = 0, 
    CCNET_TYPE_PING = 1, 
    CCNET_TYPE_PONG = 2,
}; 

struct ccnet_hdr { 
    uint16_t src; 
    uint16_t dst; 
    uint8_t  ttl; 
    uint8_t  type; 
    uint16_t len; 
    uint16_t checksum; 
} __attribute__((packed));

struct ccnet_send_parameter {
    uint16_t dst;
    uint8_t ttl;
    uint8_t type;
};

typedef int (*ccnet_link_t)(void *ctx, void *data, size_t len);

int ccnet_init(uint32_t src, uint32_t max_len);
void ccnet_graph_set_edge(int u, int v, int w);

int ccnet_register_node_link(uint32_t node_id, void *fun);
void ccnet_recompute_effective_graph(void);


int ccnet_input(void *ctx, void *data, int len);

int ccnet_output(void *ctx, void *data, int len);

#endif
