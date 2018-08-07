#include <stdio.h>                     /* for printf() */
#include <assert.h>                    /* for assert() */
#include "fult.h"

static inline int worker_id() {
  return fworker_self()->id;
}
#include "comm_exp.h"

// #define SILENT_ARGPARSING
// #include "argparsing.h"
// #include "log.h"

static long validation[] = {
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

struct fib_s {
  long val;
  long ret;
};


int i = 0;
int max = 0;
int pending = 0;
void* fib(void *arg_)
{
  long n = (long) arg_;
  record(n);

#ifdef PROFILE
  int cur = __sync_fetch_and_add(&pending, 1);
  if (cur > max) max = cur;
#endif

  if (n < 2) {
    record(n);
    return (void*) n;
  }

  long n1 = n - 1;
  long n2 = n - 2;
  fthread_t t1;
  fthread_t t2;

  // printf("%d [%p] spawning %d\n", fworker_self()->id, tlself.thread, n);
  record(n);
  // begin sched code.
  fspawn_work(fib, (void*) n1, &t1);
  fspawn_work(fib, (void*) n2, &t2);

  // printf("%d [%p] joining %d %p %p\n", fworker_self()->id, tlself.thread, n, t1, t2);

  long ret1 = (long) fthread_join(&t1);
  long ret2 = (long) fthread_join(&t2);
  // end sched code.
  record(n);
  // printf("%d [%p] done %d\n",fworker_self()->id, tlself.thread, n);

#ifdef PROFILE
  __sync_fetch_and_add(&pending, -1);
#endif
  record(n);
  return (void*) (ret1 + ret2);
}

int main(int argc, char *argv[])
{
  fult_init(atoi(argv[1]));

  long n = atoi(argv[2]);
  // memset(profs, 0, sizeof(profs));

  double t1 = -wtime();
  fthread_t root_thread;
  fspawn_work(fib, (void*) n, &root_thread);
  long ret = (long) fthread_join(&root_thread);
  t1 += wtime();

  if (validation[n] == ret) {
    record_print();
    printf("%.5f %d\n", t1, max);
    // LOG_FIB_YAML(n, ret, qtimer_secs(timer))
    // LOG_ENV_QTHREADS_YAML()
  } else {
    printf("Fail %lu (== %lu) in %f sec\n", ret, validation[n], t1); //qtimer_secs(timer));
  }

  return 0;
}

/* vim:set expandtab */
