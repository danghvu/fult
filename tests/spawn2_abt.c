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
ABT_pool* g_pool;

int NINIT = 128;
int count = 0;
int max = 0;
int total_count = 0;
int init = 0;
int stop = 0;
__thread unsigned _seed = -1;

void sum(void* a)
{
  if ((_seed == -1)) _seed = rand();

  if (a) {
    __sync_fetch_and_add(&init, 1);
    while (init < NINIT) 
      ABT_thread_yield();
  }
  if (count >= 2*NINIT) { 
    stop = 1;
  }
  if (stop) goto done;

  int rank;
  ABT_xstream_self_rank(&rank);
  usleep(rand_r(&_seed) % 10);

  while (!stop && rand_r(&_seed) % 2 > 0) {
    ABT_thread_create(g_pool[rank], sum, 0, ABT_THREAD_ATTR_NULL, NULL);
    __sync_fetch_and_add(&count, 1);
    usleep(rand_r(&_seed) % 10);
  }

done:
  __sync_fetch_and_sub(&count, 1);
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
    NINIT = argc > 2 ? atoi(argv[2]) : N;
    // printf("# of ESs: %d %d\n", num_xstreams, NINIT);
    g_pool = malloc(sizeof(ABT_pool) * num_xstreams);

    /* initialization */
    ABT_init(argc, argv);

    /* shared pool creation */
    for (i = 0; i < num_xstreams; i++) {
      ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_SPSC, ABT_TRUE,
          &g_pool[i]);
    }

    /* ES creation */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_BASIC,
                                     1, &g_pool[0]);
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_create_basic(ABT_SCHED_BASIC, 1, &g_pool[i],
                                 ABT_SCHED_CONFIG_NULL, &xstreams[i]);
        ABT_xstream_start(xstreams[i]);
    }

    ABT_thread child[NINIT];
    double total = 0;

    for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
      count = NINIT;
      double t1 = wtime(); 
      for (i = 0; i < NINIT; i++) {
        ABT_thread_create(g_pool[i % num_xstreams], sum, (void*) 1, ABT_THREAD_ATTR_NULL, &child[i]);
      }
      for (i = 0; i < NINIT; i++) {
        ABT_thread_join(child[i]);
        ABT_thread_free(&child[i]);
      }
      while (count > 0) ABT_thread_yield();
      t1 = wtime() - t1;

      if (t >= SKIP_LARGE) {
        total += t1;
      }
    }
    printf("%.5f\n", 1e3 * total / TOTAL_LARGE);

    /* join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    free(xstreams);
    free(g_pool);

    return EXIT_SUCCESS;
}
