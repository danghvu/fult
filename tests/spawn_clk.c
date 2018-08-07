#include <unistd.h>
#include <stdlib.h>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "comm_exp.h"

double _tt = 0;

void* sum(void* a)
{
  usleep(10);
  return 0;
}

#if 0
#undef TOTAL_LARGE
#undef TOTAL_SKIP
#define TOTAL_LARGE 10
#define TOTAL_SKIP 0
#endif

int main(int argc, char** args) {
  int cntr = 0;
  int NSPAWN = atoi(args[1]);
  double total = 0, max = -1e9, min = 1e9;
  int val;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    val = 0;
    double t1 = wtime(); _tt = 0;

    for (int i = 0; i < NSPAWN; i++)
      _Cilk_spawn sum(&i);

    _Cilk_sync;

    t1 = wtime() - t1;

    if (t >= SKIP_LARGE) {
      if (t1 > max) { max = t1; } // printf("%.5f %.5f\n", max * 1e3, _tt * 1e3);}
      if (t1 < min) min = t1;
      total += t1;
    }
  }
  printf("%.5f %.5f %.5f\n", 1e3 * total / TOTAL_LARGE, max*1e3, min*1e3);
  return 0;
}
