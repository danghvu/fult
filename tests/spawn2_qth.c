#include <unistd.h>
#include <stdlib.h>
#include <qthread/qthread.h>
#include "comm_exp.h"

int NINIT = 128;
int count = 0;
int max = 0;
int stop = 0;
int total_count = 0;
int init = 0;
__thread unsigned _seed = -1;

void* sum(void* a)
{
  if ((_seed == -1)) _seed = rand();

  if (a) {
    __sync_fetch_and_add(&init, 1);
    while (init < NINIT) 
      qthread_yield();
  }
  if (count >= 2*NINIT) { 
    stop = 1;
  }
  if (stop) goto done;
  usleep(rand_r(&_seed) % 10);

  aligned_t child;
  while (!stop && rand_r(&_seed) % 2 > 0) {
    qthread_fork(sum, 0, &child);
    __sync_fetch_and_add(&count, 1);
    usleep(rand_r(&_seed) % 10);
  }

done:
  __sync_fetch_and_sub(&count, 1);
  return 0;
}

#if 0
#undef TOTAL_LARGE
#undef TOTAL_SKIP
#define TOTAL_LARGE 50
#define TOTAL_SKIP 0
#endif

int main(int argc, char** args) {
  int n_worker = atoi(args[1]);
  NINIT = atoi(args[2]);
  qthread_init(n_worker);
  aligned_t* child = malloc(sizeof(aligned_t) * NINIT);
  double total = 0, max = -1e9, min = 1e9;
  int val;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    val = 0;
    count = NINIT;
    double t1 = wtime(); 
    for (int i = 0; i < NINIT; i++)
      qthread_fork(sum, 1, &child[i]);
    for (int i = 0; i < NINIT; i++)
      qthread_readFF(NULL, &child[i]);
    while (count > 0) qthread_yield();
    t1 = wtime() - t1;

    if (t >= SKIP_LARGE) {
      if (t1 > max) { max = t1; };  // printf("%d \n", count);
      if (t1 < min) min = t1;
      total += t1;
    }
  }
  printf("%.5f \n", 1e3 * total / TOTAL_LARGE);
  return 0;
}
