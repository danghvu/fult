#include <unistd.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include "comm_exp.h"

double _tt = 0;
int NINIT;
int NSPAWN;

void* sum(void* a)
{
  return 0;
}

#if 0
#undef TOTAL_LARGE
#undef TOTAL_SKIP
#define TOTAL_LARGE 1
#define TOTAL_SKIP 0
#endif

void* master_fn(void* a)
{
  aligned_t* thread = malloc(sizeof(aligned_t) * NSPAWN);
  double total = 0;
  double total_l1 = 0;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    long long l1 = -wl1miss();
    double t1 = wtime();
    for (int i = 0; i < NSPAWN; i++)
      qthread_fork_to(sum, (void*) i, &thread[i], NINIT - 1);

    for (int i = 0; i < NSPAWN; i++) {
      qthread_readFF(NULL, &thread[i]);
    }

    t1 = wtime() - t1;
    l1 += wl1miss();

    if (t >= SKIP_LARGE) {
      total += t1;
      total_l1 += l1;
    }
  }
  free(thread);
  printf("%.5f %.5f\n", 1e6 * total / NSPAWN/ TOTAL_LARGE, total_l1 / NSPAWN / TOTAL_LARGE);
  return 0;
}

int main(int argc, char** args) {
  NINIT = atoi(args[1]);
  NSPAWN = atoi(args[2]);
  qthread_init(NWORKER);
  master_fn(0);
  // aligned_t master_th;
  // qthread_fork_to(master_fn, 0, &master_th, 0);
  // qthread_readFF(NULL, &master_th);
  return 0;
}
