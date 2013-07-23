/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _TMASCO_H_
#define _TMASCO_H_

#include <tmasco_support.h>

#include <setjmp.h>
#include <errno.h>

#ifdef TMASCO_DEBUG
#define D(...) printf(__VA_ARGS__)
#else
#define D(...)
#endif

void  tmasco_begin(uintptr_t bp);
int   tmasco_switched();
void  tmasco_switch();
void  tmasco_commit();
void* tmasco_malloc(size_t size);
void* tmasco_other(void* addr);

#define __asco_begin(x)  __tmasco_begin(x)
#define __asco_switch(x) __tmasco_switch(x)
#define __asco_commit(x) __tmasco_commit(x)
#define __asco_end(x)    __tmasco_switch(x); __tmasco_commit(x)

#ifdef TMASCO_ENABLED
#include <stdint.h>

static inline uintptr_t getbp() __attribute__((always_inline));
static inline uintptr_t getbp() {
    uintptr_t rbp;
    asm __volatile__ ("mov %%rbp, %0": "=m" (rbp));
    return rbp;
}

#define __tmasco_begin(X) { /* asco scope */         \
    D("Begin(%s): %s:%d\n", #X, __FILE__, __LINE__); \
asco##X:                                             \
tmasco_begin(getbp());                               \
__transaction_atomic {

#define __tmasco_switch(X) }                              \
        if (!tmasco_switched()) { /* switching */         \
        D("Switch(%s): %s:%d\n", #X, __FILE__, __LINE__);


#define __tmasco_commit(X)                            \
    tmasco_switch();                                  \
    goto asco##X;                                     \
    } /* !switching */                                \
    D("Commit(%s): %s:%d\n", #X, __FILE__, __LINE__); \
    tmasco_commit();                                  \
    } /* !asco scope */

#else

#define __tmasco_begin(X)  __transaction_atomic {
#define __tmasco_switch(X) }
#define __tmasco_commit(X)

#endif

#endif /* _TMASCO_H_ */
