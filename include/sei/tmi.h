/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _TMI_H_
#define _TMI_H_

#ifndef SEI_DISABLED
#define SEI_ENABLED
#ifndef SEI_CLOG
# define COW_ROPURE
#endif
#endif

#define SEI_HANDLE_INIT -1

#ifdef  TMI_INSTR
#define SEI_ENABLED
#define TMI_DISABLE_OUTPUT_CHECKS
#define TMI_DISABLE_INPUT_CHECKS
#define TMI_DISABLE_PROTECTION
#endif

#ifndef SEI_ENABLED
#define TMI_DISABLE_OUTPUT_CHECKS
#define TMI_DISABLE_INPUT_CHECKS
#define TMI_DISABLE_PROTECTION
#define TMI_DISABLE_IGNORE
#endif

#include "support.h"
#include <stdint.h>
#include <setjmp.h>
#include <errno.h>

#ifdef TMI_DEBUG
#define D(...) printf(__VA_ARGS__)
#else
#define D(...)
#endif

#ifdef SEI_ENABLED
#ifdef SEI_CLOG
#define SEI_RONLY __attribute__((transaction_safe))
#else
#define SEI_RONLY __attribute__((transaction_pure))
#endif
#define SEI_PURE __attribute__((transaction_pure))
#define SEI_SAFE __attribute__((transaction_safe))

#else
#define SEI_PURE
#define SEI_SAFE
#define SEI_RONLY
#endif


void  __sei_begin(uintptr_t bp);
int   __sei_switched();
void  __sei_switch();
void  __sei_commit();
void* __sei_malloc(size_t size);
void* __sei_other(void* addr);
int   __sei_prepare(const void* ptr, size_t size, uint32_t crc, int ro);
void  __sei_prepare_nm();
int   __sei_prepare_n(const void* ptr, size_t size, uint32_t crc, int ro, int redundancy_level);
void  __sei_prepare_nm_n(int redundancy_level);
int   __sei_prepare_core(const void* ptr, size_t size, uint32_t crc, int ro);
void  __sei_prepare_nm_core(void);
int   __sei_shift(int handle);
int   __sei_bar();

void     __sei_output_append(const void* ptr, size_t size) SEI_PURE;
void     __sei_output_done() SEI_PURE;
uint32_t __sei_output_next();

void __sei_ignore_addr(void* start, void* end) SEI_PURE;
void __sei_ignore_all(uint32_t v) SEI_PURE;
void __sei_ignore(int v) SEI_PURE;

#ifdef TMI_DISABLE_OUTPUT_CHECKS
#define __tmi_output_append(ptr, size)
#define __tmi_output_done()
#define __tmi_output_next() 0
#else
#define __tmi_output_append(ptr, size) __sei_output_append(ptr, size)
#define __tmi_output_done() __sei_output_done()
#define __tmi_output_next() __sei_output_next()
#endif

#ifdef TMI_DISABLE_PROTECTION
#define __tmi_unprotect(ptr, size)
#else
void  __sei_unprotect(void* addr, size_t size);
#define __tmi_unprotect(ptr, size) __sei_unprotect(ptr,size)
#endif

#ifdef TMI_DISABLE_INPUT_CHECKS
#define __tmi_prepare(ptr, size, crc, ro) 1
#define __tmi_prepare_nm(ptr)
#define __tmi_prepare_n(ptr, size, crc, ro, n) 1
#define __tmi_prepare_nm_n(n)
#define __tmi_prepare_core(ptr, size, crc, ro) 1
#define __tmi_prepare_nm_core()
#else
#define __tmi_prepare(ptr, size, crc, ro) __sei_prepare(ptr, size, crc, ro)
#define __tmi_prepare_nm(ptr) __sei_prepare_nm()
#define __tmi_prepare_n(ptr, size, crc, ro, n) __sei_prepare_n(ptr, size, crc, ro, n)
#define __tmi_prepare_nm_n(n) __sei_prepare_nm_n(n)
#define __tmi_prepare_core(ptr, size, crc, ro) __sei_prepare_core(ptr, size, crc, ro)
#define __tmi_prepare_nm_core() __sei_prepare_nm_core()
#endif

#ifdef TMI_DISABLE_IGNORE
#define __tmi_ignore_addr(start, end) 
#define __tmi_ignore_all(v) 
#define __tmi_ignore(v)
#else
#define __tmi_ignore_addr(start, end) __sei_ignore_addr(start, end)  
#define __tmi_ignore_all(v) __sei_ignore_all(v) 
#define __tmi_ignore(v) __sei_ignore(v)
#endif

#if defined(TMI_INSTR)

#define __tmi_begin(X)  __transaction_atomic {
#define __tmi_end(X) }

#elif defined(SEI_ENABLED)

#define __tmi_begin(X) D("Begin(%s): %s:%d\n", #X, __FILE__, __LINE__); \
    __transaction_atomic {
#define __tmi_end(X) } D("Commit(%s): %s:%d\n", #X, __FILE__, __LINE__);

#else

#define __tmi_begin(X)
#define __tmi_end(X)

#endif

#endif /* _TMI_H_ */
