FLT_INLINE
unsigned fworker_randn(fworker* w)
{
    unsigned s = (214013*w->steal_seed+2531011); 
    w->steal_seed = s;
    return s;
}

FLT_INLINE
fworker_t random_worker(fworker *w) {
  for (;;) {
    unsigned s = fworker_randn(w) % nworker;
    if (s != w->id)
      return &worker[s];
  }
}

FLT_INLINE
fthread* fworker_work_done(fthread* f, int stealing)
{
  fthread* next = NULL;

  if (f->req_state == INVALID) {
#ifdef USE_COMPLEX_JOIN
    if (f->parent) {
#ifdef USE_YIELD_BACK
      next = f->parent;
#endif
      fthread_resume(f->parent);
    }
#endif
    if (f->cntr) { fcntr_signal(f->cntr); }
    // It is important to return a threads early for locality.
    fthread_fini(f);  
  } else { 
    if (f->req_state == YIELD_TO)
      next = f->yield_to;
    f->state = YIELD;
#ifndef USE_STEAL_BIT
    if (stealing) fthread_resume(f);
#endif
  } 
  return next;
}

#if 1
FLT_INLINE fthread* fworker_work_done_steal(fworker* w, fworker* steal, fthread* f)
{
  if (f->req_state == INVALID || f->req_state == YIELD_TO || steal->id != FID2WID(f->fid)) {
    return fworker_work_done(f, 1);
    // only handle YIELD
  } else { // otherwise worked on a stale thread..
    // If the thread is yielding back, we make a "copy" by swapping with a new fthread.
    // These are **slowish** code, but hopefully executed "rarely" enough
    {
      unsigned long origin_fid = f->fid;
      fthread* new_f = fworker_get_free_thread(w);
      unsigned long new_f_fid = new_f->fid;

      // Place a new thread into the stolen thread.
      new_f->state = INVALID;
      new_f->fid = origin_fid;
      steal->threads[FID2TID(origin_fid)] = new_f;

      // Place the current thread into current worker.
      w->threads[FID2TID(new_f_fid)] = f;
      f->fid = new_f_fid;
      f->state = YIELD;

      // Always schedule it afterward.
      fworker_sched_thread(w, FID2TID(new_f_fid)); 
    }
  }
  return NULL;
}
#endif

FLT_INLINE
int fworker_work(fworker* w, fthread* f, int stealing)
{
  int state = f->state;
#ifdef USE_STEAL
  // When using steal, the thiefs and worker
  // may be working on the same fthread.
  if (state != YIELD || (state = __sync_val_compare_and_swap(&f->state, YIELD, RUNNING)) != YIELD) {
#ifdef USE_STEAL_BIT
    if (state == RUNNING) fthread_resume(f);
#endif
    return 0;
  }
#else
  if (state != YIELD) return 0;
#endif

  tlself.thread = f;

  swap_ctx(&w->ctx, &f->ctx, f);

  return 1;
}

FLT_INLINE
int find_steal(fworker* steal, fworker* w, int disp, int layer)
{
  if (layer == 0) {
    fthread* f = steal->threads[disp];
    while (f) {
      if (fworker_work(w, f, 1)) {
        f = fworker_work_done(f, 1);
      } else {
        break;
      }
    }
    return 1;
  }

  disp = MUL8(disp);
  register unsigned long mask = 0;
  int succ = 0;
  unsigned long* my_mask = &steal->mask[layer-1][disp];

  for (int i = 0; i < 8; i++) {
    if (my_mask[i]) {
#ifdef USE_STEAL_BIT
      if (layer == 1) {
        mask = exchange(0, &my_mask[i]);
      } else
#endif
      mask = my_mask[i];

      while (mask > 0) {
        int ii = find_first_set(mask);
        mask = bit_flip(mask, ii);
        succ += find_steal(steal, w, MUL64(disp + i) + ii, layer - 1);
      }

#ifdef USE_STEAL_ONCE
      if (succ)
        return succ;
#else
      // Exit after worked on half of the queue.
      if (succ > (steal->thread_size - steal->thread_pool_last) / 4) {
        return succ;
      }
#endif
    }
  }
  return succ;
}

FLT_INLINE
int find_work(fworker* w, int disp, int layer)
{
  if (layer == 0) {
    fthread* f = w->threads[disp];
    while (f) {
      if (fworker_work(w, f, 0)) {
        f = fworker_work_done(f, 0);
      } else {
        break;
      }
    }
    return 1;
  }

  disp = MUL8(disp);
  int has_work = 0;
  unsigned long* my_mask = &(w->mask[layer-1][disp]);

  for (int i = 7; i >=0; i--) {
    if (my_mask[i]) {
      unsigned long mask = exchange(0, &my_mask[i]);
      while (mask > 0) {
        int ii = find_first_set(mask);
        mask = bit_flip(mask, ii);
        has_work |= find_work(w, MUL64(disp + i) + ii, layer - 1);
      }
    }
  }
  return has_work;
}

void* wfunc(void* arg)
{
  fworker* w = (fworker*)arg;
  wfunc_init(w);
  int count = 0;

  while (unlikely(!w->stop)) {
    if (!find_work(w, 0, NLAYER) && count++ > STEAL_POLL) {
#ifdef USE_STEAL
      find_steal(random_worker(w), w, 0, NLAYER);
#endif
      count = 0;
    }
  }
  return 0;
}


