#pragma once
#include "fult.h"
#include <vector>
#include <memory>

template<typename Func>
void* wrapper(void* args);

extern int nworker;

template <typename Func>
class fxthread {
 public:
  inline fxthread(Func f) : fp_(f) {}

  inline void spawn(fthread_t* handle) {
    fspawn_work((ffunc) wrapper<Func>, (void*) this, handle);
  }

  inline void spawn(fthread_t* handle, int to) {
    fspawn_to(to, (ffunc) wrapper<Func>, (void*) this, handle);
  }

  inline void call() { fp_(); }

 private:
   Func fp_;
};

template<typename Func>
void* wrapper(void* args)
{
  fxthread<Func>* f = (fxthread<Func>*) args;
  f->call();
  delete f;
  return 0;
}

template <class F>
void fspawn(const F& func, fthread_t* handle)
{
  auto f = new fxthread<F>(func);
  f->spawn(handle);
}

template <class F>
void fspawn(const F& func, fthread_t* handle, int to)
{
  auto f = new fxthread<F>(func);
  f->spawn(handle, to % nworker);
}

template <class F>
inline void fspawn_loop(F& func, size_t first, size_t last, size_t chunk) {
  if (last - first <= chunk) {
    for (size_t i = first; i < last; i++) {
      func(i);
    }
  } else {
    int c = 0;
    std::vector<fthread_t> h1((last - first  + chunk - 1) / chunk);
    while (first < last) {
      size_t next = std::min(first + chunk, last);
      fspawn([&, first, next, chunk] { 
          for (size_t i = first; i < next; i++) {
            func(i);
          }
      }, &h1[c++]);
      first += chunk;
    }
    for (int i = 0; i < c; i++) fthread_join(&h1[i]);
  }
}

template <class F>
inline void fspawn_loop_init(F& func, size_t count, size_t chunk) {
  if (count <= chunk || count < nworker) {
    for (size_t i = 0; i < count; i++) {
      func(i);
    }
  } else {
    size_t schunk = count / nworker;
    size_t first; size_t last;
    std::vector<fthread_t> h1(nworker);
    for (size_t i = 0; i < nworker; i++) {
      first = i * schunk; 
      fspawn([&, first, chunk] {
        fspawn_loop(func, first, first + schunk, chunk);
      }, &h1[i], i);
    }
    if (first + schunk < count) {
      for (size_t i = first + schunk; i < count; i++) {
        func(i);
      }
    }
    for (int i = 0; i < nworker; i++)
      fthread_join(&h1[i]);
  }
}

template <class F>
inline void fspawn_loop2(const F& func, size_t first, size_t last, size_t chunk) {
  if (last - first <= chunk) {
    func(first, last);
  } else {
    int c = 0;
    std::vector<fthread_t> h1((last - first  + chunk - 1) / chunk);
    while (first < last) {
      size_t next = std::min(first + chunk, last);
      fspawn([&, first, next, chunk] { 
        func(first, next);
      }, &h1[c++]);
      first += chunk;
    }
    for (int i = 0; i < c; i++) fthread_join(&h1[i]);
  }
}

template <class F>
inline void fspawn_loop_init2(const F & func, const size_t start, const size_t stop) {
  size_t count = stop - start;
  if (count < nworker) {
    func(start, stop);
  } else {
    size_t schunk = count / nworker;
    size_t first = start; 
    std::vector<fthread_t> h1(nworker);
    for (size_t i = 0; i < nworker; i++) {
      fspawn([&, first, schunk] {
        fspawn_loop2(func, first, first + schunk, std::max(schunk/8,(size_t) 1));
        // func(first, first + schunk);
      }, &h1[i], i);
      first += schunk;
    }
    if (first < stop) {
      func(first, stop);
    }
    for (int i = 0; i < nworker; i++)
      fthread_join(&h1[i]);
  }
}

template <typename T>
void ft_loop_balance(size_t   start,
                     size_t   stop,
                     const T &obj)
{                                       /*{{{ */
  fspawn_loop_init2(obj, start, stop);
}                                       /*}}} */

#define FFOR(from, to, step) { \
  size_t __count = (to - from) / step; size_t __chunk = std::min((size_t) 2048, std::max((size_t)1, (size_t) ((__count + nworker - 1)/nworker/8)));\
  fspawn_loop_init([&](size_t i) {\

#define FFOR_1(from, to, step) { \
  size_t __count = (to - from) / step; size_t __chunk = std::min((size_t) 1, (size_t) __count); \
  fspawn_loop_init([&](size_t i) {\

#define FFOR_256(from, to, step) { \
  size_t __count = (to - from) / step; size_t __chunk = std::min((size_t) 256, (size_t) __count); \
  fspawn_loop_init([&](size_t i) {\

#define FFOREND }, __count, __chunk); }
