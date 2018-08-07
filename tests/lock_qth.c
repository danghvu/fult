#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "comm_exp.h"

aligned_t mutex;
long CYCLE;
#define NSPIN 1000
#define SKIP 100
int a = 0;

static aligned_t lock(void* m)
{
  double* t1 = (double*) m;
  for (int i = 0; i < NSPIN; i++) {
    qthread_lock(&mutex);
    busywait_cyc(CYCLE);
    qthread_unlock(&mutex);
  }
  return 0;
}

int main(int   argc,
    char *argv[])
{
  int status;
  status = qthread_init(atoi(argv[1]));
  long n = atoi(argv[2]);
  CYCLE = atoi(argv[3]);
  assert(status == QTHREAD_SUCCESS);

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

  double std = compute_std(t, sum, n);

  printf("%.5f\n", NSPIN * n / t1);

  return EXIT_SUCCESS;
}
