#include "fult.h"
#include "comm_exp.h"

fbarrier_t b;
int NINIT, NSPAWN;
volatile int count = 0;
#define NSPIN 1000

void* barrier(void* a)
{
  for (int i = 0; i < NSPIN; i++)
    fbarrier_wait(&b);
  return 0;
}

#if 0
#undef TOTAL_LARGE
#undef TOTAL_SKIP
#define TOTAL_LARGE 1
#define TOTAL_SKIP 0
#endif

int main(int argc, char** args) {
  NINIT = atoi(args[1]);
  NSPAWN = atoi(args[2]);
  fult_init(NINIT);
  fbarrier_init(&b, NSPAWN);

  fthread_t* thread = malloc(sizeof(fthread_t) * NSPAWN);
  double t1 = -wtime();
  for (int i = 0; i < NSPAWN; i++)
    fspawn_to(i % NINIT, barrier, 0, &thread[i]);
  for (int i = 0; i < NSPAWN; i++) {
    fthread_join(&thread[i]);
  }
  t1 += wtime();
  printf("%.5f\n", NSPIN * NSPAWN / t1);

  free(thread);
  return 0;
}
