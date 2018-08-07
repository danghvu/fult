#include "fult.h"
#include "comm_exp.h"

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

int main(int argc, char** args) {
  int cntr = 0;
  int NINIT = atoi(args[1]);
  int NSPAWN = atoi(args[2]);
  fult_init(NINIT);

  fthread_t* thread = malloc(sizeof(fthread_t) * NSPAWN);
  double total = 0, max = -1e9, min = 1e9;
  int val;
  double total_l1 = 0;
  for (int t = 0; t < TOTAL_LARGE + SKIP_LARGE; t++) {
    val = 0;
    long long l1 = -wl1miss();
    double t1 = wtime();
    for (int i = 0; i < NSPAWN; i++)
      fspawn_to(i % nworker, sum, 0, &thread[i]);
    for (int i = 0; i < NSPAWN; i++) {
      fthread_join(&thread[i]);
    }
    t1 = wtime() - t1;
    l1 += wl1miss();

    if (t >= SKIP_LARGE) {
      total += t1;
      total_l1 += (double) l1;
    }
  }
  free(thread);
  printf("%.5f %.5f\n", 1e6 * total / NSPAWN / TOTAL_LARGE, total_l1 / NSPAWN / TOTAL_LARGE);
  return 0;
}
