#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "comm_exp.h"

double t1 = 0;
double l1 = 0;
volatile int spawned = 0;
volatile int turn = 0;
aligned_t feb_ping, feb_pong;

static aligned_t greeter(void *arg)
{
  printf("Hello World! My input argument was %lu\n", (unsigned long)(uintptr_t)arg);

  return 0;
}

static aligned_t ping(void* a)
{
  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 2)
    qthread_yield();

  for (int i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) {
      t1 -= wtime();
      l1 -= (double) wl1miss();
    }
    qthread_fill(&feb_pong);
    while (turn == 0)
      qthread_readFE(NULL, &feb_ping);
    turn = 0;
  }
  t1 += wtime();
  l1 += (double) wl1miss();
  return 0;
}

static aligned_t pong(void* a)
{
  __sync_fetch_and_add(&spawned, 1);
  while (spawned < 2)
    qthread_yield();

  for (int i = 0; i < TOTAL + SKIP; i++) {
    while (turn == 1)
      qthread_readFE(NULL, &feb_pong);
    turn = 1;
    qthread_fill(&feb_ping);
  }
  return 0;
}
int main(int   argc,
    char *argv[])
{
  int status;
  status = qthread_init(atoi(argv[1]));
  assert(status == QTHREAD_SUCCESS);
  wl1miss();

  aligned_t ret1, ret2;

  for (int i = 1; i < atoi(argv[1]); i++) {
    spawned = 0;
    t1 = 0; l1 = 0;
    qthread_fork_to(ping, NULL, &ret1, 0);
    qthread_fork_to(pong, NULL, &ret2, i);
    qthread_readFF(NULL, &ret1);
    qthread_readFF(NULL, &ret2);
    printf("%d %.5f\n", i, l1 / TOTAL / 2);
  }

  return EXIT_SUCCESS;
}
