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

extern asco_t* __asco;

void* tmasco_malloc(size_t size);
void* tmasco_other(void* addr);

#define TMASCO_ENABLED
#ifdef TMASCO_ENABLED
#define tmasco_begin(X) { /* asco scope */         \
    printf("Begin: %s:%d\n", __FILE__, __LINE__);  \
    volatile int asco_first;                       \
    asco_first = 1;                                \
    asco##X:                                       \
    asco_begin(__asco);                            \
    __transaction_relaxed {

#define tmasco_switch(X) }                         \
    if (asco_first) { /* switching */              \
    printf("Switch: %s:%d\n", __FILE__, __LINE__);


#define tmasco_commit(X)                           \
    asco_first = 0;                                \
    asco_switch(__asco);                           \
    goto asco##X;                                  \
    } /* !switching */                             \
    printf("Commit: %s:%d\n", __FILE__, __LINE__); \
    asco_commit(__asco);                           \
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
    printf("Commit: %s:%d\n", __FILE__, __LINE__); \
    asco_commit(__asco);                           \
    } /* !asco scope */

#else

#define tmasco_begin()  asco_begin(__asco); __transaction_relaxed {
#define tmasco_switch() }
#define tmasco_commit() asco_commit(__asco);

#endif

#endif /* _TMASCO_H_ */
