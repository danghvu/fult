#include "comm_exp.h"
#include <cilk/cilk.h>
#include <assert.h>

__thread double fine = 0;

int main(int argc, char** args) {
  double t1 = wtime();
  int n = atoi(args[1]);
  int count = 0;
  int *a = (int*) malloc(n * sizeof(int));

  cilk_for (int i = 0; i < n; i++) {
    fine -= wtime();
    // if (x == -1) x = i;
    // if (x == 0) printf("%d\n", i);
    a[i] = 1;
    fine += wtime();
  };

  printf("%.5f %.5f %d\n", wtime() - t1, fine, count);

  for (int i = 0; i < n; i++)
    assert(a[i] == 1);

  return 0;
}
