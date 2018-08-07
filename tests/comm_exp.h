#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <sys/time.h>
#include <pthread.h>
#include "math.h"

#define LARGE 8192
#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

#define NEXP 10

#define TOTAL 1000
#define SKIP 100

#define TOTAL_LARGE 1000
#define SKIP_LARGE 100

#define DEFAULT_NUM_WORKER 4
#define DEFAULT_NUM_THREAD 4

inline double wtime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec + t1.tv_usec / 1e6;
}

inline double wutime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec * 1e6 + t1.tv_usec;
}

#define MAX(a, b) ((a > b) ? (a) : (b))

#define NWORKER 8

#ifdef PROFILE
typedef struct {
  double wtimes[4096*32];
  int vals[4096*32];
  int nwtime;
} prof_t;
__thread prof_t* prof = NULL;
prof_t profs[NWORKER];
#endif

static inline void record(int n)
{
#ifdef PROFILE
  if (prof == NULL) {
    prof = &profs[(int)worker_id()];
    prof->nwtime = 0;
  }
  prof->vals[prof->nwtime] = n;
  prof->wtimes[prof->nwtime++] = wtime();
#endif
}

static inline void record_print()
{
#ifdef PROFILE
  double min_time = profs[0].wtimes[0];
  for (int i = 0; i < NWORKER; i++) {
    if (profs[i].nwtime && min_time > profs[i].wtimes[0])
      min_time = profs[i].wtimes[0];
  }
  printf("%.5f\n", min_time);

  for (int i = 0; i < NWORKER; i++) {
    printf("%d: ", i);
    for (int j = 0; j < profs[i].nwtime; j++) {
      printf("%.6f %d ", (profs[i].wtimes[j] - min_time) * 1e6, profs[i].vals[j]);
    }
    printf("\n");
  }
#endif
}

static inline double compute_std(double *t1, double sum, int len)
{
  double avg = sum / len;
  double std = 0;
  for (int i = 0; i < len; i++)
    std += (t1[i] - avg) * (t1[i] - avg);
  return sqrt(std);
}

static inline unsigned long long get_rdtsc()
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  unsigned long long cycle =
      ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
  return cycle;
}

static inline void busywait_cyc(unsigned long long delay)
{
  unsigned long long start_cycle, stop_cycle, start_plus_delay;
  start_cycle = get_rdtsc();
  start_plus_delay = start_cycle + delay;
  do {
    stop_cycle = get_rdtsc();
  } while (stop_cycle < start_plus_delay);
}

#ifdef USE_PAPI
#include <papi.h>
static int EV[1] = { PAPI_TOT_INS };
static int prof_started = 0;
static long long prof_cur = 0;

static inline long long wl1miss() {
  if (unlikely(!prof_started)) {
    PAPI_thread_init(pthread_self);
    PAPI_start_counters(EV, 1);
    prof_started = 1;
  }
  PAPI_accum_counters(&prof_cur, 1); 
  return prof_cur;
}
#else
static inline long long wl1miss() {
  return 0;
}
#endif

#ifdef __cplusplus
#include <mutex>
#include <condition_variable>
class cpp_barrier
{
 private:
  std::mutex _mutex;
  std::condition_variable _cv;
  std::size_t _count;

 public:
  explicit cpp_barrier(std::size_t count) : _count{count} {}
  void wait()
  {
    std::unique_lock<std::mutex> lock{_mutex};
    if (--_count == 0) {
      _cv.notify_all();
    } else {
      _cv.wait(lock, [this] { return _count == 0; });
    }
  }
};
#endif
#endif
