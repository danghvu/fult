#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/barrier.h>

#include "comm_exp.h"

aligned_t mutex;
long CYCLE;
#define NSPIN 1000
#define SKIP 100
int a = 0;

qt_barrier_t* barrier;

static aligned_t lock(void* m)
{
  for (int i = 0; i < NSPIN; i++) {
    qt_barrier_enter(barrier);
  }
  return 0;
}

int main(int   argc,
    char *argv[])
{
  int status;
  status = qthread_init(atoi(argv[1]));
  long n = atoi(argv[2]);
  assert(status == QTHREAD_SUCCESS);

  barrier = qt_barrier_create (n, LOOP_BARRIER);

  aligned_t* th = malloc(sizeof(aligned_t) * n);
  double *t = malloc(sizeof(double) * n);
  double min = 1e9, max = 0, sum = 0;

  double t1 = -wtime();
  for (int i = 0; i < n; i++) {
    qthread_fork_to(lock, &t[i], &th[i], i);
  }

  for (int i = 0; i < n; i++) {
    qthread_readFF(NULL, &th[i]);
  }
  t1 += wtime();

  printf("%.5f\n", NSPIN * n / t1);

  return EXIT_SUCCESS;
}
