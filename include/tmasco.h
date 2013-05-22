/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _TMASCO_H_
#define _TMASCO_H_
#include <asco.h>
#include <setjmp.h>
#include <errno.h>

void* tmasco_malloc(size_t size);

#ifdef TMASCO_ENABLED
#define tmasco_begin() { /* asco scope */       \
    volatile int asco_first;                    \
    asco_first = 1;                             \
    jmp_buf asco_buf;                           \
    asco_first = 1 - setjmp(&asco_buf);         \
    asco_begin(__asco);                         \
    __transaction_relaxed {

#define tmasco_switch() }                       \
        printf("first: %d\n", asco_first);      \
        if (asco_first) { /* switching */       \
        asco_switch(__asco);

#define tmasco_commit()                         \
    longjmp(&asco_buf, 1);                      \
    } /* !switching */                          \
    asco_commit(__asco);                        \
    } /* !asco scope */

#else

#define tmasco_begin()  asco_begin(__asco); __transaction_relaxed {
#define tmasco_switch() }
#define tmasco_commit() asco_commit(__asco);

#endif

#endif /* _TMASCO_H_ */
