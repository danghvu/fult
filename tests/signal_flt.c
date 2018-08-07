#include "fult.h"
#include "comm_exp.h"

double t1 = 0;
double l1 = 0;
volatile int spawned = 0;
volatile int turn = 0;

#if 0
#undef TOTAL
#define TOTAL 10
#undef SKIP
#define SKIP 0
#endif

fbarrier_t b;

void* ping(void* a)
{
  fbarrier_wait(&b);
  fthread_t ft = *((fthread_t*) a);

  for (int i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) { 
      l1 -= (double) wl1miss();
      t1 -= wtime();
    }
    fthread_resume(ft);
    while (turn == 0)
      fthread_wait();
    turn = 0;
  }
  t1 += wtime();
  l1 += (double) wl1miss();

  fbarrier_wait(&b);
  return 0;
}

void* pong(void* a)
{
  fbarrier_wait(&b);

  fthread_t ft = (*(fthread_t*) a);

  for (int i = 0; i < TOTAL + SKIP; i++) {
    while (turn == 1)
      fthread_wait();
    turn = 1;
    fthread_resume(ft);
  }

  fbarrier_wait(&b);
  return 0;
}

int main(int argc, char** args) {
  int n = atoi(args[1]);
  fult_init(n);
  fthread_t ping_thread, pong_thread;
  fbarrier_init(&b, 2);
  spawned = 0;
  for (int i = 1; i < n; i++) {
    t1 = 0; l1 = 0;
    fworker_spawn_to(0, ping, (void*) &pong_thread, F_STACK_SIZE, &ping_thread);
    fworker_spawn_to(i, pong, (void*) &ping_thread, F_STACK_SIZE, &pong_thread);
    fthread_join(&ping_thread);
    fthread_join(&pong_thread);

    printf("%d %.5f\n", i, 1e6 * t1 / TOTAL / 2);
    fflush(stdout);
  }
  fult_finalize();
  return 0;
}
