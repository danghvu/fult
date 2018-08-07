#include "fult.h"
#include "comm_exp.h"

int NINIT = 128;
int count = 0;
int init = 0;
int stop = 0;
int total_count = 0;
__thread unsigned _seed = -1;

void* sum(void* a)
{
  if (_seed == -1) _seed = rand();
  if (a) {
    __sync_fetch_and_add(&init, 1);
    while (init < NINIT) 
      fthread_yield();
  }
  if (count >= 2*NINIT) { 
    stop = 1;
  }
  if (stop == 1) goto done;

  usleep(rand_r(&_seed) % 10);

  fthread_t child;
  while (!stop && rand_r(&_seed) % 2 > 0) {
    fworker_spawn(sum, 0, &child);
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
  fult_init(n_worker);
  fthread_t child[NINIT];
  double total = 0;
  double total_l1 = 0;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    count = NINIT;
    double t1 = wtime(); 
    long long l1 = wl1miss();
    for (int i = 0; i < NINIT; i++)
      fworker_spawn_to(i % n_worker, sum, 1, F_STACK_SIZE, &child[i]);
    for (int i = 0; i < NINIT; i++)
      fthread_join(&child[i]);
    while (count > 0) fthread_yield();
    t1 = wtime() - t1;
    l1 = wl1miss() - l1;

    if (t >= SKIP_LARGE) {
      total += t1;
      total_l1 += (double) l1;
    }
  }
  printf("%.5f\n", 1e6 * total / NINIT / TOTAL_LARGE);
  return 0;
}
