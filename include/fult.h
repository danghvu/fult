#ifndef _FULT_H_
#define _FULT_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "config.h"

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#define memfence() { asm volatile("": : :"memory"); }

#define WORDSIZE (8 * sizeof(long))

#define FLT_EXPORT __attribute__((visibility("default")))
#define FLT_INLINE static inline __attribute__((always_inline))

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MAKE_FID(wid, tid) ((((unsigned long) (wid)) << 32) | ((unsigned long) (tid)))
#define FID2TID(fid) (fid & 0x00ffffffff)
#define FID2WID(fid) (fid >> 32)

#define MUL64(x) ((x) << 6)
#define DIV64(x) ((x) >> 6)
#define DIV512(x) ((x) >> 9)
#define DIV32768(x) ((x) >> 15)
#define MUL8(x) ((x) << 3)
#define MOD_POW2(x, y) ((x) & ((y)-1))

#if NLAYER == 2
#define NMASK (8 * 8 * 64)
#else
#define NMASK (8)
#endif

#define MAX_THREAD (WORDSIZE * NMASK)
#define NUM_INIT_THREAD (64)

#define ALIGN64(x) (((uintptr_t) (x) + 64 - 1) / 64 * 64)

#define LC_SPIN_UNLOCKED 0
#define LC_SPIN_LOCKED 1

#include "bitops.h"
#include "config.h"

#define DEBUG(x)

typedef void* (*ffunc)(void*);
typedef void* fcontext_t;

struct fthread;
typedef struct fthread fthread;
typedef struct fthread* fthread_t;

struct fworker;
typedef struct fworker fworker;
typedef struct fworker* fworker_t;

typedef struct fthread* fsync_t;

struct tls_t {
  struct fthread* thread;
  struct fworker* worker;
};

typedef struct fctx {
  struct fctx* parent;
  fcontext_t stack_ctx;
} fctx;

enum fthread_state {
  INVALID = 0,
  YIELD,
  YIELD_TO,
  RUNNING,
};

typedef struct {
  unsigned count;
  fthread_t waiter;
} fcntr_t;

typedef struct {
  volatile int lock __attribute__((aligned(64)));
  fthread_t* waiters;
  unsigned max_wait;
  unsigned first, last, flag;
} fmutex_t;

typedef struct {
  fmutex_t mutex;
  fthread_t* waiters;
  unsigned init_count;
  unsigned left;
} fbarrier_t;

#ifdef __cplusplus
extern "C" {
#endif

FLT_EXPORT
void fult_init(int);

FLT_EXPORT
void fult_finalize();

FLT_INLINE
void fspawn(ffunc f, void* data, fthread_t*);

FLT_EXPORT
void fworker_create(fworker*, int affinity);

FLT_EXPORT
void fworker_destroy(fworker*);

FLT_INLINE
int fworker_id(fworker* w);

FLT_INLINE 
fthread_t fthread_self();

FLT_INLINE
fworker_t fworker_self();

FLT_INLINE
void fthread_resume(fthread* f);

FLT_INLINE
void fthread_yield();

FLT_INLINE
void fthread_wait();

FLT_INLINE
void* fthread_join(fthread_t* f);

/** Implementation from here. */

// fcontext (from boost).
fcontext_t make_fcontext(void* sp, size_t size, void (*thread_func)(void*));
void* jump_fcontext(fcontext_t* old, fcontext_t, void* arg);

FLT_INLINE int ftrylock(volatile int* flag)
{
  if (__sync_lock_test_and_set(flag, 1)) {
    return 0;
  }
  return 1;
}

FLT_INLINE void flock(volatile int* flag)
{
  if (__sync_lock_test_and_set(flag, 1)) {
    while (1) {
      while (*flag) {
        asm("pause;");
      }
      if (!__sync_val_compare_and_swap(flag, 0, 1)) break;
    }
  }
}

FLT_INLINE void flock_yield(volatile int* flag)
{
  if (__sync_lock_test_and_set(flag, 1)) {
    while (1) {
      while (*flag) {
        fthread_yield();
      }
      if (!__sync_val_compare_and_swap(flag, 0, 1)) break;
    }
  }
}

FLT_INLINE void funlock(volatile int* flag) { __sync_lock_release(flag); }

FLT_INLINE void swap_ctx_noparent(fctx* from, fctx* to, void* args)
{
  jump_fcontext(&(from->stack_ctx), to->stack_ctx, args);
}

FLT_INLINE void swap_ctx(fctx* from, fctx* to, void* args)
{
  to->parent = from;
  jump_fcontext(&(from->stack_ctx), to->stack_ctx, args);
}

FLT_INLINE void swap_ctx_parent(fctx* f)
{
  jump_fcontext(&(f->stack_ctx), f->parent->stack_ctx, f->parent);
}

struct fthread {
  // modify everytime spawn.
  fctx ctx;
  ffunc func;
  void* stack;
  void* data;
  struct fthread* parent;
  struct fthread* yield_to;
  fcntr_t*  cntr;
  fthread_t* uthread;

  struct {
    enum fthread_state req_state;
    volatile enum fthread_state state;
  } __attribute__((aligned(64)));

  // write once - readonly.
  volatile unsigned long fid __attribute__((aligned(64)));
} __attribute__((aligned(64)));

struct fworker {
  // readonly or private.
  unsigned long* mask[2];
  unsigned int id;
  unsigned int steal_seed;
  pthread_t runner;
  int stop;

  // mostly read only.
  unsigned thread_size;
  fthread** threads;

  // semi private.
  fctx ctx __attribute__((aligned(64)));

  // volatile.
  struct {
    unsigned int* thread_pool;
    unsigned thread_pool_last;
    volatile int thread_pool_lock;
  } __attribute__((aligned(64)));
} __attribute__((aligned(64)));

extern fthread main_sched;
extern fthread_t main_thread;
extern __thread struct tls_t tlself;
extern struct fworker* worker;

void* wfunc(void*);
typedef void* (*pthread_fn_t)(void *);

#include "abti_mem.h"

FLT_INLINE int fworker_id(fworker* w) { return w->id; }

FLT_INLINE fthread_t fthread_self() { return tlself.thread; }

FLT_INLINE fworker_t fworker_self() { return tlself.worker; }

FLT_INLINE void add_more_threads(fworker* w, int num_threads)
{
#if 0
  if (w->thread_pool_last == 0) {
    for (int i = 0; i < w->thread_size; i++) {
      if (w->threads[i]->state == INVALID) {
        w->thread_pool[w->thread_pool_last++] = i;
      }
    }
  }
  if (w->thread_pool_last > 0) {
    return;
  }
#endif

  // fprintf(stderr, "[FULT] [%d] More thread %d\n", w->id, num_threads);

  int thread_size = w->thread_size;
  // extends the pool.
  void* new_pool = realloc(w->thread_pool, sizeof(unsigned int) * (thread_size + num_threads));
  assert(new_pool && "out of mem");
  w->thread_pool = (unsigned int *) new_pool;

  for (int i = 0; i < num_threads; i++) {
    unsigned tid = thread_size + (num_threads - i - 1);
    fthread* t = ABTI_mem_alloc_thread(); 
    assert(t && t->stack && "no more memory\n");
    t->state = INVALID;
    t->fid = MAKE_FID(w->id, tid);
    w->threads[tid] = t;
    w->thread_pool[w->thread_pool_last++] = tid;
    assert(tid < MAX_THREAD);
  }
  w->thread_size += num_threads;
  assert(w->thread_size < MAX_THREAD);
}


FLT_INLINE
fthread* fworker_get_free_thread(fworker* w)
{
  flock(&w->thread_pool_lock);
  while (w->thread_pool_last == 0) {
    add_more_threads(w, MIN(w->thread_size, MAX_THREAD - 1 - w->thread_size));
    if (w->thread_pool_last == 0) {
      funlock(&w->thread_pool_lock);
      fthread_yield();
      flock(&w->thread_pool_lock);
    }
  }
  unsigned int id = w->thread_pool[--w->thread_pool_last];
  fthread* t = w->threads[id];
  funlock(&w->thread_pool_lock);
  return t;
}


FLT_INLINE void fworker_start(fworker* w)
{
  w->stop = 1; // will set to 0 when worker is ready to go.
  pthread_create(&w->runner, 0, (pthread_fn_t) wfunc, w);
  while (w->stop) sched_yield();
}

FLT_INLINE void fworker_stop(fworker* w)
{
  w->stop = 1;
  pthread_join(w->runner, 0);
}

FLT_INLINE void fworker_sched_thread(fworker* w, const unsigned long id)
{
  sync_set_bit(MOD_POW2(id, WORDSIZE), &w->mask[0][DIV64(id)]);
#if (NLAYER == 2)
  sync_set_bit(DIV512(MOD_POW2(id, 32768)), &w->mask[1][DIV32768(id)]);
#endif
}

void fwrapper(void*);

FLT_INLINE
void fthread_create(fthread* f, ffunc func, void* data,
                              size_t stack_size)
{
  if (unlikely(f->stack == NULL || stack_size > F_STACK_SIZE)) {
    void* memory = 0;
    posix_memalign(&memory, 64, stack_size);
    memset(memory, 0, stack_size);
    if (memory == 0) {
      fprintf(stderr, "No more memory for stack\n");
      exit(EXIT_FAILURE);
    }
    f->stack = memory;
  }

#ifdef USE_VALGRIND
  VALGRIND_STACK_REGISTER(f->stack, (char*) f->stack + stack_size);
#endif

  f->func = func;
  f->data = data;
  f->ctx.stack_ctx = make_fcontext((char*) f->stack + stack_size,
                                   stack_size, fwrapper);

  // Important fence... we don't want to publish the state 
  // while all the information is not yet in placed.
  memfence();
  f->state = YIELD;
}

FLT_INLINE
void _fspawn(fworker* w, ffunc func, void* data, size_t stack_size, fthread_t* thread, fcntr_t* cntr)
{
  fthread* f = fworker_get_free_thread(w);
  f->uthread = thread;
#ifdef USE_COMPLEX_JOIN
  if (tlself.worker == w) {
    f->parent = tlself.thread;
  }
  else
#endif
    f->parent = NULL;
  f->cntr = cntr;

  f->yield_to = 0;

  // may pass NULL.
  if (thread) *thread = f;

  // add it to the fthread.
  fthread_create(f, func, data, stack_size);

  // make it schedable.
  fworker_sched_thread(w, FID2TID(f->fid));
}

FLT_INLINE
void fthread_yield_to(fthread* to);

FLT_INLINE
void _fspawn_work(fworker* w, ffunc func, void* data, size_t stack_size, fthread_t* thread, fcntr_t* cntr)
{
  fthread* f = fworker_get_free_thread(w);
  f->uthread = thread;
#ifdef USE_COMPLEX_JOIN
  if (tlself.worker == w) {
    f->parent = tlself.thread;
  }
  else
#endif
    f->parent = NULL;
  f->cntr = cntr;

  // may pass NULL.
  if (thread) *thread = f;

  // add it to the fthread.
  fthread_create(f, func, data, stack_size);

  fthread_yield_to(f);
}

FLT_INLINE
void fspawn_to(int id, ffunc func, void* data, fthread_t* thread)
{
  _fspawn(&worker[id], func, data, F_STACK_SIZE, thread, NULL);
}

FLT_INLINE
void fspawn(ffunc func, void* data, fthread_t* thread)
{
  _fspawn(tlself.worker, func, data, F_STACK_SIZE, thread, NULL);
}

FLT_INLINE
void fspawn_on(int id, ffunc func, void* data, fthread_t* thread, fcntr_t* cntr)
{
  _fspawn(&worker[id], func, data, F_STACK_SIZE, thread, cntr);
}

FLT_INLINE
void fspawn_work(ffunc func, void* data, fthread_t* thread)
{
  _fspawn_work(tlself.worker, func, data, F_STACK_SIZE, thread, NULL);
}

FLT_INLINE
void fspawn_to2(int id, ffunc func, void* data, fthread_t* thread)
{
  if (id == tlself.worker->id)
    _fspawn_work(tlself.worker, func, data, F_STACK_SIZE, thread, NULL);
  else 
    _fspawn(&worker[id], func, data, F_STACK_SIZE, thread, NULL);
}

FLT_INLINE
void fspawn_work_on(ffunc func, void* data, fthread_t* thread, fcntr_t* cntr)
{
  _fspawn_work(tlself.worker, func, data, F_STACK_SIZE, thread, cntr);
}

FLT_INLINE
void fthread_yield_to(fthread* to)
{
  fthread* f = tlself.thread;
  f->req_state = YIELD_TO;
  f->yield_to = to;
  fthread_resume(f);
  swap_ctx_parent(&f->ctx);
}

FLT_INLINE void fthread_resume(fthread* f)
{
  fworker_sched_thread(&worker[FID2WID(f->fid)], FID2TID(f->fid));
}

FLT_INLINE void fthread_yield()
{
  fthread* f = tlself.thread;
  f->req_state = YIELD;
  fthread_resume(f);
  swap_ctx_parent(&f->ctx);
}

FLT_INLINE void fthread_wait()
{
  fthread* f = tlself.thread;
  f->req_state = YIELD;
  swap_ctx_parent(&f->ctx);
}

FLT_INLINE void* fthread_join(fthread_t* f)
{
  fthread_t ff = *f;
  int count = 0;
  if (ff == tlself.thread) return NULL;

  while ( (((unsigned long)*f) & 0x1) == 0) {

#ifdef USE_COMPLEX_JOIN
    if (ff->parent == tlself.thread) {
      fthread_wait();
    } else
#endif
      fthread_yield();
  }
#if 1
  return (void*) (((unsigned long) (*f)) >> 1);
#endif
  return 0;
}

FLT_INLINE void fcntr_init(fcntr_t* fcnt, int count) {
  fcnt->count = count;
  fcnt->waiter = tlself.thread;
}

FLT_INLINE void fcntr_wait(fcntr_t* fcnt)
{
  while ( fcnt->count ) {
    fthread_wait();
  }
}

FLT_INLINE void fcntr_signal(fcntr_t* fcnt)
{
  if (__sync_fetch_and_sub(&fcnt->count, 1) - 1 == 0)
    fthread_resume(fcnt->waiter);
}

FLT_INLINE
void fmutex_init(fmutex_t *m)
{
#define MAX_WAIT 64 
  m->waiters = (fthread_t*) malloc(MAX_WAIT * sizeof(fthread_t));
  m->lock = 0;
  m->flag = 0;
  m->max_wait = MAX_WAIT;
  m->first = m->last = 0;
}

FLT_INLINE
void fmutex_lock(fmutex_t *m)
{
  if (__sync_lock_test_and_set(&m->flag, 1)) {
    flock(&m->lock);
    if (m->flag) {
      m->waiters[m->last++] = fthread_self();
      m->last &= (m->max_wait-1);
      if (unlikely(m->last == m->first)) {
        m->first = 0;
        m->last = m->max_wait;
        m->waiters = (fthread_t*) realloc(m->waiters, m->max_wait * 2 * sizeof(fthread_t));
        m->max_wait *= 2;
      }
      funlock(&m->lock);
      fthread_wait();
    } else {
      m->flag = 1;
      funlock(&m->lock);
    }
  }
}

FLT_INLINE
void fmutex_unlock(fmutex_t *m)
{
  flock(&m->lock);
  if (m->first != m->last) {
    fthread_resume(m->waiters[m->first++]);
    m->first &= (m->max_wait-1);
  } else {
    m->first = m->last = 0;
    // last one, no one is waiting, set flag.
    m->flag = 0;
  }
  funlock(&m->lock);
}

FLT_INLINE
void fbarrier_init(fbarrier_t *b, unsigned num)
{
  b->waiters = (fthread_t*) calloc(num, sizeof(fthread_t));
  fmutex_init(&b->mutex);
  b->init_count = num;
  b->left = num;
}

FLT_INLINE
void fbarrier_wait(fbarrier_t* b) {
  fmutex_lock(&b->mutex);
  int left = __sync_fetch_and_sub(&b->left, 1) - 1;
  int init_count = b->init_count;
  if (left != 0) {
    b->waiters[left] = fthread_self();
    fmutex_unlock(&b->mutex);
    fthread_wait();
  }
  left = __sync_fetch_and_add(&b->left, 1) + 1;
  if (left == b->init_count) {
    fmutex_unlock(&b->mutex);
  } else {
    fthread_resume((fthread*) b->waiters[left]);
  }
}

extern int nworker;

FLT_INLINE
int fult_nworkers() {
  return nworker;
}

#ifdef __cplusplus
}
#endif

#endif
