#ifndef FULT_INL_H_
#define FULT_INL_H_

#include "fult.h"

#ifdef __cplusplus
extern "C" {
#endif

fthread main_sched;
fthread_t main_thread;
__thread struct tls_t tlself;
void fwrapper(void* args);
struct fworker* worker;
int nworker = 1;
volatile unsigned int waiters = 0;

FLT_INLINE int fult_set_me_to(int core_id)
{
#ifdef USE_AFFI
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

#ifdef AFF_DEBUG
  fprintf(stderr, "[USE_AFFI] Setting someone to core # %d\n", core_id);
#endif
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#else
  return 0;
#endif
}

FLT_INLINE
void fworker_start_main(fworker* w);

void fult_init(int _nworker)
{
  gp_ABTI_global = malloc(sizeof(ABTI_global));
  ABTI_mem_init(gp_ABTI_global);
  lp_ABTI_local = malloc(sizeof(ABTI_local));
  ABTI_mem_init_local(lp_ABTI_local);

  nworker = _nworker;
  worker = (struct fworker*) malloc(nworker * sizeof(struct fworker));
  for (int i = 0; i < nworker; i++) {
    worker[i].steal_seed = i;
    fworker_create(&worker[i], i);
  }
  for (int i = 1; i < nworker; i++) {
    fworker_start(&worker[i]);
  }

  fworker_start_main(&worker[0]);
}

FLT_INLINE
void fworker_stop_main(fworker* w);

void fult_finalize()
{
  while (fworker_self()->id != 0)
    fthread_yield();
  for (int i = 1; i < nworker; i++) {
    fworker_stop(&worker[i]);
  }
  fworker_stop_main(&worker[0]);
}

extern double _tt;

void fwrapper(void* args)
{
  fthread* f = (fthread*)args;
  unsigned long ret = ((unsigned long)f->func(f->data));
  // assert(ret >> 63 == 0 && "overflow return");
  ret = 0x1 | ret << 1;
  *f->uthread = (fthread_t) ret;

  f->req_state = INVALID;
  swap_ctx_parent(&f->ctx);
}

/// Fworker.
FLT_INLINE
void fworker_start_main(fworker* w)
{
  memset(w->mask[1], 0, 8 * sizeof(long));
  memset(w->mask[0], 0, NMASK * sizeof(long));

  memset(&main_sched, 0, sizeof(struct fthread));
  fthread_create(&main_sched, wfunc, w, MAIN_STACK_SIZE);
  main_sched.uthread = &main_thread;
  tlself.thread = &main_sched;
  _fspawn(w, NULL, NULL, MAIN_STACK_SIZE, &main_thread, NULL);
  main_thread->parent = NULL;
  main_thread->ctx.parent = &(main_sched.ctx);
  tlself.thread = main_thread;
  tlself.worker = w;
  fthread_yield();
}

FLT_INLINE
void fworker_stop_main(fworker* w)
{
  w->stop = 1;
  main_sched.ctx.parent = &(main_thread->ctx);
  fthread_wait();
  // free((char*) main_sched.stack);
}

void fworker_create(fworker* w, int affinity)
{
  void* mask;
#if 1
  long sz = sysconf(_SC_PAGESIZE);
  posix_memalign(&mask, sz, (8 + NMASK) * sizeof(long));
  madvise(&mask, (8 + NMASK) * sizeof(long), MADV_HUGEPAGE);
  w->mask[0] = mask;
  w->mask[1] = (void*) ((char*) mask + NMASK * sizeof(long));
#else
  posix_memalign(&mask, 64, 8 * sizeof(long));
  memset(mask, 0, 8 * sizeof(long));
  w->mask[0] = mask;
  posix_memalign(&mask, 64, 8 * WORDSIZE * sizeof(void*));
  w->mask[1] = mask;
  for (int i = 0; i < 8 * WORDSIZE; i++) {
    posix_memalign(&mask, 64, 8 * sizeof(long));
    memset(mask, 0, 8 * sizeof(long));
    w->mask[1][i] = mask;
  }
#endif

  w->id = affinity;
  w->thread_pool_last = 0;
  w->thread_pool_lock = 0;
  w->thread_size = 0;
  posix_memalign((void**) &w->threads, 64, sizeof(uintptr_t) * MAX_THREAD);
  w->thread_pool = NULL;

  flock(&w->thread_pool_lock);
  add_more_threads(w, NUM_INIT_THREAD);
  memfence();
  funlock(&w->thread_pool_lock);
}

void fworker_destroy(fworker* w)
{
  free((void*)w->thread_pool);
  free(w->threads);
}

FLT_INLINE
void fthread_fini(fthread* f)
{
  // This has a lot of cache miss when stealing. FIXME.
  f->state = INVALID;
  int wid = FID2WID(f->fid);
  int tid = FID2TID(f->fid);
  fworker* w = &worker[wid];
  flock(&w->thread_pool_lock);
  w->thread_pool[w->thread_pool_last++] = tid;
  funlock(&w->thread_pool_lock);
}

#if 0
__thread double wtimework, wtimeall;
#endif

FLT_INLINE void wfunc_init(fworker* w)
{
  // The main thread has its own initialization...
  if (w->id != 0) {
    memset(w->mask[1], 0, 8 * sizeof(long));
    memset(w->mask[0], 0, NMASK * sizeof(long));
  }

  tlself.worker = w;
  fult_set_me_to(w->id);
  if (!lp_ABTI_local) {
    lp_ABTI_local = malloc(sizeof(ABTI_local));
    ABTI_mem_init_local(lp_ABTI_local);
  }

  // initialize the thread pool.
  ABTI_mem_free_thread(ABTI_mem_alloc_thread());

  memfence();
  // ready to go.
  w->stop = 0;
}

#include "fworker_all.h"

#ifdef __cplusplus
}
#endif

#endif
