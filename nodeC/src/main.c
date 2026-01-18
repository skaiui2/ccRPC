#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ccnet.h"

static void test_simple()
{
    printf("\n=== Test 1: Simple graph ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0, 1, 1);
    ccnet_graph_set_edge(&g, 1, 2, 20);
    ccnet_graph_set_edge(&g, 2, 3, 1);
    ccnet_graph_set_edge(&g, 0, 3, 2);
    ccnet_graph_set_edge(&g, 3, 2, 1);

    int nh = ccnet_next_hop(&g, 0, 2);
    printf("next hop from 0 to 2 = %d (expected 3)\n", nh);
}

static void test_symmetric()
{
    printf("\n=== Test 2: Symmetric graph ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,0,1);
    ccnet_graph_set_edge(&g, 1,2,1);
    ccnet_graph_set_edge(&g, 2,1,1);
    ccnet_graph_set_edge(&g, 2,3,1);
    ccnet_graph_set_edge(&g, 3,2,1);

    int nh = ccnet_next_hop(&g, 0, 3);
    printf("next hop from 0 to 3 = %d (expected 1)\n", nh);
}

static void test_asymmetric()
{
    printf("\n=== Test 3: Asymmetric graph ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,2,1);
    ccnet_graph_set_edge(&g, 2,3,1);
    ccnet_graph_set_edge(&g, 3,0,100);

    int nh = ccnet_next_hop(&g, 3, 2);
    printf("next hop from 3 to 2 = %d (expected 0)\n", nh);
}

static void test_unreachable()
{
    printf("\n=== Test 4: Unreachable graph ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,0,1);

    ccnet_graph_set_edge(&g, 2,3,1);
    ccnet_graph_set_edge(&g, 3,2,1);

    int nh = ccnet_next_hop(&g, 0, 3);
    printf("next hop from 0 to 3 = %d (expected -1)\n", nh);
}

static void test_loop()
{
    printf("\n=== Test 5: Loop graph ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 3);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,2,1);
    ccnet_graph_set_edge(&g, 2,0,1);

    int nh = ccnet_next_hop(&g, 0, 2);
    printf("next hop from 0 to 2 = %d (expected 1)\n", nh);
}

static void test_equal_paths()
{
    printf("\n=== Test 6: Equal-cost multipath ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,3,1);
    ccnet_graph_set_edge(&g, 0,2,1);
    ccnet_graph_set_edge(&g, 2,3,1);

    int nh = ccnet_next_hop(&g, 0, 3);
    printf("next hop from 0 to 3 = %d (expected 1 or 2)\n", nh);
}

static void test_extreme_weights()
{
    printf("\n=== Test 7: Extreme weights ===\n");

    ccnet_graph g;
    ccnet_graph_init(&g, 4);

    ccnet_graph_set_edge(&g, 0,1,1);
    ccnet_graph_set_edge(&g, 1,2,10000);
    ccnet_graph_set_edge(&g, 0,3,2);
    ccnet_graph_set_edge(&g, 3,2,1);

    int nh = ccnet_next_hop(&g, 0, 2);
    printf("next hop from 0 to 2 = %d (expected 3)\n", nh);
}

static void test_random()
{
    printf("\n=== Test 8: Random stress test ===\n");

    srand(time(NULL));

    for (int t = 0; t < 10; t++) {
        ccnet_graph g;
        ccnet_graph_init(&g, 6);

        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                if (i != j && rand() % 3 == 0)
                    ccnet_graph_set_edge(&g, i, j, rand() % 20 + 1);

        int nh = ccnet_next_hop(&g, 0, 5);
        printf("random test %d: next hop = %d\n", t, nh);
    }
}

int main(void)
{
    test_simple();
    test_symmetric();
    test_asymmetric();
    test_unreachable();
    test_loop();
    test_equal_paths();
    test_extreme_weights();
    test_random();

    return 0;
}
