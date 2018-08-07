#include "fult.h"
#include "comm_exp.h"

__thread double t1 = 0;
__thread double l1 = 0;
volatile int spawned = 0;
volatile int turn = 0;

#if 0
#undef TOTAL
#define TOTAL 10
#undef SKIP
#define SKIP 0
#endif

typdef struct {
  fthread_t waiters[64];
  int count;
} waitlist_t;

void waitlist_wait(waitlist_t* w)
{
  int cur = __sync_fetch_and_add(&w->count, 1);
  w->waiters[cur] = fthread_self();
}

void waitlist_signal(waitlist_t* w)
{
  for (int i = 0; i < w->count; i++)
    fthread_resume(w->waiters[i]);
  w->count = 0;
}

void* ping(void* a)
{
  waitlist_t* w = (waitlist_t*) a;

  for (int i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) { 
      t1 -= wtime();
      l1 -= (double) wl1miss();
    }
    waitlist_wait(w);
  }
  t1 += wtime();
  l1 += (double) wl1miss();

  return 0;
}

void* pong(void* a)
{
  waitlist_t* w = (waitlist_t*) a;

  for (int i = 0; i < TOTAL + SKIP; i++) {
    waitlist_signal(w);
  }
  return 0;
}

int main(int argc, char** args) {
  int n = atoi(args[1]);
  fult_init(n);
  fthread_t ping_thread, pong_thread;
  spawned = 0;
  waitlist_t wait;
  wait.count = 0;

  for (int i = 1; i < n; i++) {
    double total = 0;
    t1 = 0; l1 = 0;
    fworker_spawn_to(0, ping, (void*) &wait, F_STACK_SIZE, &ping_thread);
    fworker_spawn_to(i, pong, (void*) &wait, F_STACK_SIZE, &pong_thread);
    fthread_join(&ping_thread);
    fthread_join(&pong_thread);

    printf("%d %.5f\n", i, l1 / TOTAL / 2);
    fflush(stdout);
  }
  fult_finalize();
  return 0;
}
