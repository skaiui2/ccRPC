#include "ccnet.h"

void ccnet_graph_init(ccnet_graph *g, int n)
{
    g->node_count = n;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            g->cost[i][j] = (i == j) ? 0 : CCNET_INF;
}

void ccnet_graph_set_edge(ccnet_graph *g, int u, int v, int w)
{
    g->cost[u][v] = w;
}

void ccnet_dijkstra_run(ccnet_graph *g, int src, ccnet_dijkstra *st)
{
    int n = g->node_count;

    for (int i = 0; i < n; i++) {
        st->dist[i] = CCNET_INF;
        st->prev[i] = -1;
        st->visited[i] = 0;
    }
    st->dist[src] = 0;

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        int best = CCNET_INF;

        for (int i = 0; i < n; i++)
            if (!st->visited[i] && st->dist[i] < best) {
                best = st->dist[i];
                u = i;
            }

        if (u == -1)
            break;

        st->visited[u] = 1;

        for (int v = 0; v < n; v++) {
            int w = g->cost[u][v];
            if (w >= CCNET_INF) continue;
            if (st->dist[u] + w < st->dist[v]) {
                st->dist[v] = st->dist[u] + w;
                st->prev[v] = u;
            }
        }
    }
}

int ccnet_next_hop(ccnet_graph *g, int src, int dst)
{
    ccnet_dijkstra st;
    ccnet_dijkstra_run(g, src, &st);

    if (st.dist[dst] >= CCNET_INF)
        return -1;

    int cur = dst;
    int p = st.prev[cur];

    if (p == -1)
        return -1;

    while (p != src && p != -1) {
        cur = p;
        p = st.prev[cur];
    }

    if (p == -1)
        return -1;

    return cur;
}
