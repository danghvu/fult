#include <stdio.h>                     /* for printf() */
#include <assert.h>                    /* for assert() */
#include "fult.h"

#include "comm_exp.h"

unsigned int a = 0;

#define NSPIN 1000
#define SKIP 100
fmutex_t mutex;
long CYCLE;

void* lockunlock(void* m) {
  double* t1 = (double *)m;
  for (int i = 0; i < NSPIN; i++) {
    fmutex_lock(&mutex);
    busywait_cyc(CYCLE);
    fmutex_unlock(&mutex);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  fult_init(atoi(argv[1]));
  long n = atoi(argv[2]);
  CYCLE = atoi(argv[3]);

  fthread_t* th = malloc(sizeof(fthread_t) * n);
  fmutex_init(&mutex);
  double sum = 0;
  double *t = malloc(sizeof(double) * n);
  double t1 = -wtime();

  for (int i = 0; i < n; i++) 
    fspawn_to(i % nworker, lockunlock, (void*) &t[i], &th[i]);

  for (int i = 0; i < n; i++) {
    fthread_join(&th[i]);
  }

  t1 += wtime();

  printf("%.5f\n", NSPIN * n / t1);

  return 0;
}

/* vim:set expandtab */
