/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of tasks to compute Fibonacci numbers. The
 * execution proceeds in two phases.  1) Expand phase. A binary tree of
 * activation records is built in a top-down fashion. Each activation record
 * corresponds to a Fibonacci number Fib(n) computed recursively. A task
 * computing Fib(n) will create two subtasks to compute Fib(n-1) and Fib(n-2),
 * respectively.  2) Aggregrate phase. The final results is computed bottom-up.
 * Once a base case is reached (n<=2), a task is created to aggregate the result
 * into the parent's activation record. Therefore, two tasks will update the
 * activation record for Fib(n). The last of the two aggregate tasks will spawn
 * another task that will aggregate the result up the binary tree.
 */
#include <unistd.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <abt.h>
#include "comm_exp.h"

#define N               10
#define NUM_XSTREAMS    4

/* global variables */
ABT_pool* pools;

int NSPAWN = 128;
int max = 0;

void sum(void* a)
{
}

/* Main function */
int main(int argc, char *argv[])
{
    int n, i, result, expected;
    int num_xstreams;
    ABT_xstream *xstreams;

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        // printf("Usage: %s [N=10] [num_ES=4]\n", argv[0]);
        return EXIT_SUCCESS;
    }
    num_xstreams = argc > 1 ? atoi(argv[1]) : NUM_XSTREAMS;
    NSPAWN = argc > 2 ? atoi(argv[2]) : N;
    // printf("# of ESs: %d %d\n", num_xstreams, NSPAWN);
    pools = malloc(sizeof(ABT_pool) * num_xstreams);

    /* initialization */
    ABT_init(argc, argv);

    for (i = 0; i < num_xstreams; i++) {
      ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
          &pools[i]);
    }

    /* ES creation */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ABT_xstream_self(&xstreams[0]);
#ifdef RANDWS
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_RANDWS,
                                     num_xstreams, &pools[0]);
#else
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_BASIC,
                                     1, &pools[0]);
#endif

    for (i = 1; i < num_xstreams; i++) {
#ifdef RANDWS
        ABT_xstream_create_basic(ABT_SCHED_RANDWS, num_xstreams, &pools[0],
                                 ABT_SCHED_CONFIG_NULL, &xstreams[i]);
#else
        ABT_xstream_create_basic(ABT_SCHED_BASIC, 1, &pools[i],
                                 ABT_SCHED_CONFIG_NULL, &xstreams[i]);
#endif
    }

    /* Get the first pool associated with each ES */
    // for (i = 0; i < num_xstreams; i++) {
    //    ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    // }

    ABT_thread* child = malloc(sizeof(ABT_thread) * NSPAWN);
    double total = 0;
    double total_l1 = 0;
#if 1
    for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
      long long l1 = -wl1miss();
      double t1 = wtime(); 
      for (i = 0; i < NSPAWN; i++) {
        ABT_thread_create(pools[num_xstreams - 1], sum, (void*) 1,
                          ABT_THREAD_ATTR_NULL, &child[i]);
      }
      for (i = 0; i < NSPAWN; i++) {
        ABT_thread_join(child[i]);
        ABT_thread_free(&child[i]); // this makes thing faster!
      }
      t1 = wtime() - t1;
      l1 += wl1miss();

      if (t >= SKIP_LARGE) {
        total += t1;
        total_l1 += (double) l1;
      }
    }
#endif
    printf("%.5f %.5f\n", 1e6 * total / NSPAWN/ TOTAL_LARGE, total_l1 / NSPAWN / TOTAL_LARGE);

    /* join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();
    
    free(child);
    free(xstreams);
    free(pools);

    return EXIT_SUCCESS;
}
