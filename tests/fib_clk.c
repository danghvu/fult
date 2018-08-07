#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <stdio.h>                     /* for printf() */
#include <assert.h>                    /* for assert() */
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <qthread/qthread.h>
#include <qthread/qtimer.h>

static inline void set_me_to(int core_id)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

static inline int worker_id() {
  return (int) __cilkrts_get_worker_number();
}

#include "comm_exp.h"
// #include "argparsing.h"

static aligned_t validation[] = {
    0,        // 0
    1,        // 1
    1,        // 2
    2,        // 3
    3,        // 4
    5,        // 5
    8,        // 6
    13,       // 7
    21,       // 8
    34,       // 9
    55,       // 10
    89,       // 11
    144,      // 12
    233,      // 13
    377,      // 14
    610,      // 15
    987,      // 16
    1597,     // 17
    2584,     // 18
    4181,     // 19
    6765,     // 20
    10946,    // 21
    17711,    // 22
    28657,    // 23
    46368,    // 24
    75025,    // 25
    121393,   // 26
    196418,   // 27
    317811,   // 28
    514229,   // 29
    832040,   // 30
    1346269,  // 31
    2178309,  // 32
    3524578,  // 33
    5702887,  // 34
    9227465,  // 35
    14930352, // 36
    24157817, // 37
    39088169  // 38
};

int pending = 0, max = 0;
__thread int id = -1;

static aligned_t fib(void *arg_)
{
    aligned_t *n = (aligned_t *)arg_;
  record(*n);

#ifdef PROFILE
  int cur = __sync_fetch_and_add(&pending, 1);
  if (cur > max) max = cur;
#endif

    if (*n < 2) {
      record(*n);
        return *n;
    }

    aligned_t ret1, ret2;
    aligned_t n1 = *n - 1;
    aligned_t n2 = *n - 2;
    record(*n);

    ret1 = _Cilk_spawn fib(&n1);
    ret2 = _Cilk_spawn fib(&n2);

    _Cilk_sync;
    record(*n);
#ifdef PROFILE
    __sync_fetch_and_add(&pending, -1);
#endif
    record(*n);
    return ret1 + ret2;
}

int main(int   argc,
         char *argv[])
{
    qtimer_t  timer = qtimer_create();
    __cilkrts_set_param("nworkers", argv[1]);
    aligned_t n     = atoi(argv[2]);
    aligned_t ret   = 0;

    /* setup */
    // CHECK_VERBOSE();
    // NUMARG(n, "FIB_INPUT");

    qtimer_start(timer);
    ret = _Cilk_spawn fib(&n);
    _Cilk_sync;
    qtimer_stop(timer);

    if (validation[n] == ret) {
        record_print();
        printf("%.5f %d\n", qtimer_secs(timer), max);
        // fprintf(stdout, "%d %lu %lu %f\n", __cilkrts_get_nworkers(), (unsigned long)n, (unsigned long)ret, qtimer_secs(timer));
    } else {
        fprintf("Fail %lu (== %lu) in %f sec\n", (unsigned long)ret, (unsigned long)validation[n], qtimer_secs(timer));
    }

    qtimer_destroy(timer);

    return 0;
}

/* vim:set expandtab */
