#include <unistd.h>
#include <stdlib.h>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "comm_exp.h"

int NINIT = 128;
int count = 0;
int total_count = 0;
__thread unsigned _seed = -1;

void* sum(void* a)
{
  __sync_fetch_and_add(&count, 1);
  if (a) {
    while (count < NINIT) 
      sched_yield();
  }
  if (count >= 2*NINIT) { 
    return 0;
  }

  if (_seed == -1) _seed = rand();

  while (rand_r(&_seed) % 2 > 0) {
    _Cilk_spawn sum(0);
  }

  return 0;
}

#if 0
#undef TOTAL_LARGE
#undef TOTAL_SKIP
#define TOTAL_LARGE 10
#define TOTAL_SKIP 0
#endif

int main(int argc, char** args) {
  __cilkrts_set_param("nworkers", args[1]);
  NINIT = atoi(args[2]);

  double total = 0, max = -1e9, min = 1e9;
  int val;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    val = 0;
    count = 0;
    double t1 = wtime(); 

    for (int i = 0; i < NINIT; i++)
      _Cilk_spawn sum(1);
    _Cilk_sync;

    t1 = wtime() - t1;

    if (t >= SKIP_LARGE) {
      if (t1 > max) { max = t1; } // printf("%.5f %.5f\n", max * 1e3, _tt * 1e3);}
      if (t1 < min) min = t1;
      total += t1;
      total_count += count;
    }
  }
  printf("%.5f %d\n", 1e3 * total / TOTAL_LARGE, total_count / TOTAL_LARGE);
  return 0;
}
