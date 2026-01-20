#include "ccnet.h"
#include "in_cksum.h"
#include "hashmap.h"
#include <memory.h>
#include <arpa/inet.h>

#define TIMER_COUNT 1
#define TIMER_UPDATE_PING 0
struct ccnet_private {
    struct ccnet_graph g_eff;
    struct ccnet_dyn g_dyn; 
    struct ccnet_graph g_base;

    uint32_t ccnet_ping_ts[CCNET_MAX_NODES];
    uint32_t ccnet_ping_timeout_count[CCNET_MAX_NODES]; 
    uint32_t timer[TIMER_COUNT];
    struct hashmap ccnet_trans_fun_map;
    uint16_t src;
};

static struct ccnet_private *ccnet_pri;
static uint32_t ccnet_clock = 0;

void ccnet_recompute_effective_graph(void);

static void ccnet_debug_hex(const char *tag, const void *buf, size_t len)
{
    const uint8_t *p = buf;

    printf("---- %s (%zu bytes) ----\n", tag, len);

    for (size_t i = 0; i < len; i++) {
        printf("%02X ", p[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");

    printf("-----------------------------\n");
}


static int a = 0;
static void ccnet_debug_dump_tx(const char *reason,
                              const void *buf, size_t len)
{
/*
    printf("\n[ccnet TX] %s, a:%d\n", reason, a++);
    ccnet_debug_hex("TX Packet", buf, len); 
*/
}

static int b = 0;
static void ccnet_debug_dump_rx(const void *buf, size_t len)
{
/*
    printf("\n[ccnet RX] %d\n", b++);
    ccnet_debug_hex("RX Packet", buf, len);
*/
}

void ccnet_graph_init(struct ccnet_graph *g, int n)
{
    g->node_count = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            g->cost[i][j] = (i == j) ? 0 : CCNET_INF;
        }
    }
}

void ccnet_graph_set_edge(int u, int v, int w)
{
    struct ccnet_graph *g = &ccnet_pri->g_base;
    g->cost[u][v] = w;
}

void ccnet_dijkstra_run(struct ccnet_graph *g, int src, struct ccnet_dijkstra *st)
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

        if (u == -1) break;

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

void ccnet_recompute_effective_graph(void)
{
    int n = ccnet_pri->g_base.node_count;
    ccnet_pri->g_eff.node_count = n;

    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {

            int base = ccnet_pri->g_base.cost[u][v];
            int rtt  = ccnet_pri->g_dyn.rtt[u][v];

            if (base >= CCNET_INF) {
                ccnet_pri->g_eff.cost[u][v] = CCNET_INF;
                continue;
            }

            if (rtt >= CCNET_INF) {
                ccnet_pri->g_eff.cost[u][v] = CCNET_INF;
                continue;
            }

            if (rtt == 0) {
                ccnet_pri->g_eff.cost[u][v] = base;
                continue;
            }

            ccnet_pri->g_eff.cost[u][v] =
                (base * CCNET_BASE_WEIGHT + rtt * CCNET_RTT_WEIGHT) / 100;
        }
    }
}


int ccnet_next_hop(struct ccnet_graph *g, int src, int dst)
{
    struct ccnet_dijkstra st;
    
    if (dst < 0 || dst >= g->node_count) {
        return -1;
    }

    if (src < 0 || src >= g->node_count) {
        return -1;
    }

    ccnet_dijkstra_run(g, src, &st);

    if (st.dist[dst] >= CCNET_INF) {
        return -1;
    }

    int cur = dst;
    int p = st.prev[cur];

    if (p == -1) {
        return -1;
    }

    while (p != src && p != -1) {
        cur = p;
        p = st.prev[cur];
    }

    if (p == -1) {
        return -1;
    }

    return cur;
}


int ccnet_init(uint32_t src, uint32_t max_len)
{
    if (src >= CCNET_MAX_NODES) { 
        return -1; 
    }

    struct ccnet_private *cp = malloc(sizeof(struct ccnet_private));
    if (!cp)
        return -1;

    memset(cp, 0, sizeof(*cp));
    ccnet_pri = cp;

    cp->src = src;
    cp->timer[TIMER_UPDATE_PING] = CCNET_UPDATE_PERIOD;

    hashmap_init(&cp->ccnet_trans_fun_map, max_len, HASHMAP_KEY_INT);

    ccnet_graph_init(&cp->g_base, CCNET_MAX_NODES);
    ccnet_graph_init(&cp->g_eff, CCNET_MAX_NODES);
    memset(&cp->g_dyn, 0, sizeof(struct ccnet_dyn));

    return 0;
}

int ccnet_register_node_link(uint32_t node_id, void *fun)
{
    hashmap_put(&ccnet_pri->ccnet_trans_fun_map, node_id, fun);
    return 0;
}

void ccnet_output_data(uint16_t dst, void *data, uint16_t len)
{
    ccnet_link_t clt = hashmap_get(&ccnet_pri->ccnet_trans_fun_map, dst);
    if (!clt) {
        return;
    }
    ccnet_debug_dump_tx("CCNET_OUT", data, len);
    clt(NULL, (void *)data, len);
}


int ccnet_output(void *ctx, void *data, int len)
{
    struct ccnet_send_parameter *csp = (struct ccnet_send_parameter *)ctx;
    uint16_t dst = csp->dst;
    uint8_t ttl = csp->ttl;
    uint8_t type = csp->type;
    int next_hop = ccnet_next_hop(&ccnet_pri->g_eff, ccnet_pri->src, dst); 
    if (next_hop < 0) { 
        return -1; 
    }

    uint16_t all_len = len + sizeof(struct ccnet_hdr);
    struct ccnet_hdr *ch_all = malloc(all_len);
    if (!ch_all) {
        return -1;
    }
    *ch_all = (struct ccnet_hdr) {
        .dst = htons(dst),
        .len = htons(len),
        .src = htons(ccnet_pri->src),
        .ttl = ttl,
        .type = type,
    };
    void *payload_load = (void *)(ch_all + 1);
    if (data) {
        memcpy(payload_load, data, len);
    }
    ch_all->checksum = 0;
    ch_all->checksum = in_checksum(ch_all, all_len);
    ccnet_output_data(next_hop, ch_all, all_len);
    free(ch_all);
    return 0;
}


void ccnet_ping_output(uint16_t dst)
{
    struct ccnet_send_parameter csp;
    csp.dst = dst;
    csp.ttl = CCNET_TTL;
    csp.type = CCNET_TYPE_PING;
    ccnet_output(&csp, 0, 0);
}

void ccnet_pong_reply_output(uint16_t dst)
{
    struct ccnet_send_parameter csp;
    csp.dst = dst;
    csp.ttl = CCNET_TTL;
    csp.type = CCNET_TYPE_PONG;
    ccnet_output(&csp, 0, 0);
}

void ccnet_update_graph_rtt(uint16_t dst)
{
    if (dst >= CCNET_MAX_NODES) {
        return;
    }

    uint32_t rtt_sample = ccnet_clock - ccnet_pri->ccnet_ping_ts[dst];
    uint32_t history = ccnet_pri->g_dyn.rtt[ccnet_pri->src][dst];
    if (history == 0) {
        ccnet_pri->g_dyn.rtt[ccnet_pri->src][dst] = rtt_sample;
    } else {
        ccnet_pri->g_dyn.rtt[ccnet_pri->src][dst] = history - (history >> 3) + (rtt_sample >> 3);
    }
    ccnet_pri->ccnet_ping_timeout_count[dst] = 0; 
}


void ccnet_update_graph_output(struct ccnet_private *cp)
{
    for(int j = 0; j < CCNET_MAX_NODES; j++) {
        if (j != cp->src) {
            ccnet_pri->ccnet_ping_ts[j] = ccnet_clock;
            ccnet_ping_output(j);
            ccnet_pri->ccnet_ping_timeout_count[j]++;
            if (ccnet_pri->ccnet_ping_timeout_count[j] > 5) {
                ccnet_pri->g_dyn.rtt[cp->src][j] = CCNET_INF;
                ccnet_recompute_effective_graph();
            } 
        }
    }
}

void ccnet_timer_process(void)
{
    ccnet_clock++;
    struct ccnet_private *cp = ccnet_pri;
    if (cp->timer[TIMER_UPDATE_PING] > 0) {
        cp->timer[TIMER_UPDATE_PING]--;
        if (cp->timer[TIMER_UPDATE_PING] == 0) {
            ccnet_update_graph_output(cp);
            cp->timer[TIMER_UPDATE_PING] = CCNET_UPDATE_PERIOD;
        }
    }
}


int ccnet_route(struct ccnet_hdr *ch, void *data, int len)
{
    uint16_t src = ccnet_pri->src;
    uint16_t dst = ntohs(ch->dst);
    ch->checksum = 0;
    ch->checksum = in_checksum(data, len);

    int nh = ccnet_next_hop(&ccnet_pri->g_eff, src, dst);
    if (nh < 0) return -1;

    ccnet_output_data(nh, data, len);
    return 0;
}


void ccnet_process_data(void *data)
{
    void *payload_data = (void *)(((struct ccnet_hdr *)data) + 1);
    uint16_t payload_len = ntohs(((struct ccnet_hdr *)data)->len);
    ccnet_link_t clt = hashmap_get(&ccnet_pri->ccnet_trans_fun_map, ccnet_pri->src);
    if (!clt) { 
        return; 
    }

    clt(NULL, payload_data, payload_len);
}


int ccnet_input(void *ctx, void *data, int len)
{
    (void)ctx;
    
    if (len < sizeof(struct ccnet_hdr)) {
        return -1;
    }

    struct ccnet_hdr *ch = (struct ccnet_hdr *)data;
    uint16_t cksum = in_checksum(data, len);
    if (cksum != 0) { 
        return -1; 
    }

    ccnet_debug_dump_rx(data, len);
    uint16_t src = ntohs(ch->src);
    uint16_t dst = ntohs(ch->dst);
    ch->ttl--;
    if (ch->ttl == 0) {
        return 0;
    }
    if (dst != ccnet_pri->src) {
        if (ccnet_route(ch, data, len) < 0) {
            return -1;
        }
        return 0;
    }

    switch(ch->type) { 
    case CCNET_TYPE_DATA:
        ccnet_process_data(data);
        break;

    case CCNET_TYPE_PONG:
        ccnet_update_graph_rtt(src);
        ccnet_recompute_effective_graph();
        break;

    case CCNET_TYPE_PING:
        ccnet_pong_reply_output(dst); 
        break;
    default:
        break;
    }
    return 0;
}