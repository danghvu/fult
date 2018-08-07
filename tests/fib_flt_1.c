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
  struct fib_s* f = (struct fib_s*) arg_;
  record(f->val);

  int cur = __sync_fetch_and_add(&pending, 1);
  if (cur > max) max = cur;


  if (f->val < 2) {
    f->ret = f->val;
    record(f->val);
    return 0;
  }

  long n1 = f->val - 1;
  long n2 = f->val - 2;

  fthread_t t1, t2;
  struct fib_s f1 = {n1, 0}, f2 = {n2, 0};
  record(f->val);

  // begin sched code.
  fworker_spawn(fworker_self()->id, fib, (void*) &f1, F_STACK_SIZE, &t1);
  fworker_spawn(fworker_self()->id, fib, (void*) &f2, F_STACK_SIZE, &t2);
  fthread_join(&t1);
  fthread_join(&t2);
  // end sched code.
  record(f->val);

  f->ret = f1.ret + f2.ret;
  __sync_fetch_and_add(&pending, -1);
  record(f->val);
  return 0;
}

int main(int argc, char *argv[])
{
  fthread_t ping_thread, pong_thread;
  fult_init(NWORKER);

  long n = atoi(argv[1]);
  memset(profs, 0, sizeof(profs));

  double t1 = -wtime();
  fthread_t root_thread;
  struct fib_s root = {n, 0};
  fworker_spawn(0, fib, (void*) &root, F_STACK_SIZE, &root_thread);
  fthread_join(&root_thread);
  t1 += wtime();

  if (validation[n] == root.ret) {
    record_print();
    printf("%.5f %d\n", t1, max);
    // LOG_FIB_YAML(n, ret, qtimer_secs(timer))
    // LOG_ENV_QTHREADS_YAML()
  } else {
    printf("Fail %lu (== %lu) in %f sec\n", root.ret, validation[n], t1); //qtimer_secs(timer));
  }

  return 0;
}

/* vim:set expandtab */
