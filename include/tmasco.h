/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _TMASCO_H_
#define _TMASCO_H_

#ifdef TMASCO_INSTR
#define TMASCO_ENABLED
#define TMASCO_DISABLE_OUTPUT_CHECKS
#define TMASCO_DISABLE_INPUT_CHECKS
#define TMASCO_DISABLE_PROTECTION
#endif

#ifndef TMASCO_ENABLED
#define TMASCO_DISABLE_OUTPUT_CHECKS
#define TMASCO_DISABLE_INPUT_CHECKS
#define TMASCO_DISABLE_PROTECTION
#endif

#include <tmasco_support.h>
#include <stdint.h>
#include <setjmp.h>
#include <errno.h>

#ifdef TMASCO_DEBUG
#define D(...) printf(__VA_ARGS__)
#else
#define D(...)
#endif

#ifdef TMASCO_ENABLED
#define TMASCO_PURE __attribute__((transaction_pure));
#define TMASCO_SAFE __attribute__((transaction_safe));
#else
#define TMASCO_PURE
#define TMASCO_SAFE
#endif

void  tmasco_begin(uintptr_t bp);
int   tmasco_switched();
void  tmasco_switch();
void  tmasco_commit();
void* tmasco_malloc(size_t size);
void* tmasco_other(void* addr);
int   tmasco_prepare(const void* ptr, size_t size, uint32_t crc, int ro);
void  tmasco_prepare_nm();
int   tmasco_shift(int handle);

void     tmasco_output_append(const void* ptr, size_t size) TMASCO_PURE;
void     tmasco_output_done() TMASCO_PURE;
uint32_t tmasco_output_next();

#ifdef TMASCO_DISABLE_OUTPUT_CHECKS
#define __asco_output_append(ptr, size)
#define __asco_output_done()
#define __asco_output_next() 0
#else
#define __asco_output_append(ptr, size) tmasco_output_append(ptr, size)
#define __asco_output_done() tmasco_output_done()
#define __asco_output_next() tmasco_output_next()
#endif

#ifdef TMASCO_DISABLE_PROTECTION
#define __asco_unprotect(ptr, size)
#else
void  tmasco_unprotect(void* addr, size_t size);
#define __asco_unprotect(ptr, size) tmasco_unprotect(ptr,size)
#endif

#ifdef TMASCO_DISABLE_INPUT_CHECKS
#define __asco_prepare(ptr, size, crc, ro) 1
#define __asco_prepare_nm(ptr)
#else
#define __asco_prepare(ptr, size, crc, ro) tmasco_prepare(ptr, size, crc, ro)
#define __asco_prepare_nm(ptr) tmasco_prepare_nm()
#endif

#define __asco_begin(x)  __tmasco_begin(x)
#define __asco_switch(x) __tmasco_switch(x)
#define __asco_commit(x) __tmasco_commit(x)
#define __asco_end(x)    __tmasco_switch(x); __tmasco_commit(x)

#if defined(TMASCO_INSTR)

#define __tmasco_begin(X)  __transaction_atomic {
#define __tmasco_switch(X) }
#define __tmasco_commit(X)

#elif defined(TMASCO_ENABLED)

#define __tmasco_begin(X) D("Begin(%s): %s:%d\n", #X, __FILE__, __LINE__); \
    __transaction_atomic {
#define __tmasco_switch(X) }
#define __tmasco_commit(X) D("Commit(%s): %s:%d\n", #X, __FILE__, __LINE__);

#else

#define __tmasco_begin(X)
#define __tmasco_switch(X)
#define __tmasco_commit(X)

#endif

#endif /* _TMASCO_H_ */
