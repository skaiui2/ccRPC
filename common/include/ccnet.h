#ifndef CCNET_H
#define CCNET_H

#define CCNET_MAX_NODES 16
#define CCNET_INF 0x3fff

typedef struct {
    int cost[CCNET_MAX_NODES][CCNET_MAX_NODES];
    int node_count;
} ccnet_graph;

typedef struct {
    int dist[CCNET_MAX_NODES];
    int prev[CCNET_MAX_NODES];
    int visited[CCNET_MAX_NODES];
} ccnet_dijkstra;

void ccnet_graph_init(ccnet_graph *g, int n);
void ccnet_graph_set_edge(ccnet_graph *g, int u, int v, int w);
int ccnet_next_hop(ccnet_graph *g, int src, int dst);

#endif
