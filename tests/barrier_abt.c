#include <abt.h>
#include <stdlib.h>
#include <unistd.h>

#include "comm_exp.h"

double t1 = 0;
ABT_mutex mutex;
ABT_barrier barrier;
int a = 0;
#define NSPIN 1000
#define SKIP 100
long CYCLE;

void lock(void* m)
{
  for (int i = 0; i < NSPIN; i++) {
    ABT_barrier_wait(barrier);
  }
}

int NUM_XSTREAMS;

int main(int argc, char *argv[])
{
    NUM_XSTREAMS = atoi(argv[1]);
    long n = atoi(argv[2]);
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_pool    pools[NUM_XSTREAMS];
    ABT_thread* th = malloc(n * sizeof(ABT_thread));
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

    ABT_barrier_create(n, &barrier);
    double t1 = -wtime();

    for (int i = 0; i < n; i++) {
      ABT_thread_create(pools[i % NUM_XSTREAMS], lock, 0,
                        ABT_THREAD_ATTR_NULL, &th[i]);
    }

    for (int i = 0; i < n; i++) {
      ABT_thread_join(th[i]);
    }
    t1 += wtime();
    printf("%.5f\n", NSPIN * n / t1);

    ABT_finalize();

    return 0;
}
