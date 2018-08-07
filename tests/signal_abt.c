#include <abt.h>
#include <stdlib.h>
#include <unistd.h>

#include "comm_exp.h"

double t1 = 0;
double l1 = 0;
volatile int spawned = 0;
volatile int turn = 0;
ABT_cond ping_cond, pong_cond;
ABT_mutex ping_mtx, pong_mtx;

void ping(void* a)
{
  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 2)
    ABT_thread_yield();

  for (int i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) { 
      t1 -= wtime();
      l1 -= (double) wl1miss();
    }
    ABT_cond_signal(pong_cond);
    ABT_mutex_lock(ping_mtx);
    while (turn == 0)
      ABT_cond_wait(ping_cond, ping_mtx);
    ABT_mutex_unlock(ping_mtx);
    ABT_mutex_lock(pong_mtx);
    turn = 0;
    ABT_mutex_unlock(pong_mtx);
  }
  t1 += wtime();
  l1 += (double) wl1miss();

  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 4)
    ABT_thread_yield();
}

void pong(void* a)
{
  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 2)
    ABT_thread_yield();

  for (int i = 0; i < TOTAL + SKIP; i++) {
    ABT_mutex_lock(pong_mtx);
    while (turn == 1)
      ABT_cond_wait(pong_cond, pong_mtx);
    ABT_mutex_unlock(pong_mtx);
    ABT_mutex_lock(ping_mtx);
    turn = 1;
    ABT_mutex_unlock(ping_mtx);
    ABT_cond_signal(ping_cond);
  }

  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 4)
    ABT_thread_yield();
}

int NUM_XSTREAMS;

int main(int argc, char *argv[])
{
    NUM_XSTREAMS = atoi(argv[1]);
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_pool    pools[NUM_XSTREAMS];
    ABT_thread  ping_thread, pong_thread;
    size_t i;

    ABT_init(argc, argv);

    /* Execution Streams */
    ABT_xstream_self(&xstreams[0]);
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get the first pool associated with each ES */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    }

    ABT_cond_create(&ping_cond);
    ABT_cond_create(&pong_cond);
    ABT_mutex_create(&ping_mtx);
    ABT_mutex_create(&pong_mtx);

    for (int i = 1; i < NUM_XSTREAMS; i++) {
      spawned = 0;
      t1 = 0; l1 = 0;
      ABT_thread_create(pools[0], ping, NULL,
                        ABT_THREAD_ATTR_NULL, &ping_thread);
      ABT_thread_create(pools[i], pong, NULL,
                        ABT_THREAD_ATTR_NULL, &pong_thread);
      ABT_thread_join(ping_thread);
      ABT_thread_join(pong_thread);
      ABT_thread_free(&ping_thread);
      ABT_thread_free(&pong_thread);
      printf("%d %.5f\n", i, l1 / TOTAL / 2);
      ABT_xstream_join(xstreams[i]);
      ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    return 0;
}
