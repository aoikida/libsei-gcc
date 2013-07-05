/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _TMASCO_H_
#define _TMASCO_H_

#include <asco.h>
#include <tmasco_support.h>

#include <setjmp.h>
#include <errno.h>

#ifdef TMASCO_DEBUG
#define D(...) printf(__VA_ARGS__)
#else
#define D(...)
#endif

extern asco_t* __asco;
//extern uintptr_t**  __asco_ignore_addrs;

void* tmasco_malloc(size_t size);
void* tmasco_other(void* addr);

#ifdef TMASCO_ENABLED
#include <stdint.h>
extern uintptr_t __asco_high;

static inline uintptr_t getbp() __attribute__((always_inline));
static inline uintptr_t getbp() {
    //register const uintptr_t rbp asm ("rbp");
    //return rbp;
    uintptr_t rbp;
    asm __volatile__ ("mov %%rbp, %0": "=m" (rbp));
    return rbp;
}

#define tmasco_begin(X) { /* asco scope */           \
    D("Begin(%s): %s:%d\n", #X, __FILE__, __LINE__); \
    __asco_high = getbp();                           \
    volatile int asco_first;                         \
    asco_first = 1;                                  \
asco##X:                                             \
asco_begin(__asco);                                  \
__transaction_atomic {

#define tmasco_switch(X) }                                \
        if (asco_first) { /* switching */                 \
        D("Switch(%s): %s:%d\n", #X, __FILE__, __LINE__);


#define tmasco_commit(X)                              \
    asco_first = 0;                                   \
    asco_switch(__asco);                              \
    goto asco##X;                                     \
    } /* !switching */                                \
    D("Commit(%s): %s:%d\n", #X, __FILE__, __LINE__); \
    asco_commit(__asco);                              \
    } /* !asco scope */

#elif TMASCO_ENABLED2

#define tmasco_begin(X) { /* asco scope */      \
    volatile int asco_first;                    \
    asco_first = 1;                             \
    jmp_buf asco_buf;                           \
    asco_first = 1 - setjmp(asco_buf);          \
    asco_begin(__asco);                         \
    __transaction_relaxed {

#define tmasco_switch(X) }                      \
        if (asco_first) { /* switching */       \
        asco_switch(__asco);

#define tmasco_commit(X)                           \
    longjmp(asco_buf, 1);                          \
    } /* !switching */                             \
    D("Commit: %s:%d\n", __FILE__, __LINE__);      \
    asco_commit(__asco);                           \
    } /* !asco scope */

#else

#define tmasco_begin()  asco_begin(__asco); __transaction_relaxed {
#define tmasco_switch() }
#define tmasco_commit() asco_commit(__asco);

#endif

#endif /* _TMASCO_H_ */
