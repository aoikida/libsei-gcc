/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <asco.h>
#include "debug.h"
#include "heap.h"
#include "cow.h"

#ifndef TMASCO_ENABLED
#include "tmasco_instr.c"
#else /* TMASCO_ENABLED */

/* -----------------------------------------------------------------------------
 * tmasco state (asco object and stack boundaries)
 * -------------------------------------------------------------------------- */

asco_t* __asco;
uintptr_t __asco_low;
uintptr_t __asco_high;

/* initialize library and allocate an asco object. This is called once
 * the library is loaded. If static liked should be called on
 * initialization of the program. */
static void __attribute__ ((constructor))
tmasco_init()
{
    assert (__asco == NULL);
    __asco = asco_init();
    assert (__asco);
}

static inline uintptr_t getsp() __attribute__((always_inline));
static inline uintptr_t getsp() {
    register const uintptr_t rsp asm ("rsp");
    __asco_low = rsp; // this update is only necessary for debugging
    return rsp;
}

/* check whether address x is in the stack or not. */
#define IN_STACK(x) getsp() <= (uintptr_t) x && (uintptr_t) x < __asco_high

#if 0
extern char __data_start;
extern char __bss_start;
extern char __bss_end;
extern char edata;
extern void* __asco_ignore_addrs[];
#endif



/* addresses inside the stack are local variables and shouldn't be
 * considered when reading and writing. Other addresses can be
 * ignored, for example, global variables.
 */
static int inline ignore_addr(const void* ptr) __attribute__((always_inline));
static int inline
ignore_addr(const void* ptr)
{
    if (IN_STACK(ptr)) {
        DLOG3("Ignore address: %p\n", ptr);
        return 1;
    } else return 0;

#if 0
    if ((uintptr_t) ptr < (uintptr_t) &edata) return 1;

    if (1) {
        int i;
        for (i = 0; __asco_ignore_addrs[i]; ++i)
            if (ptr == __asco_ignore_addrs[i])
                return 1;
        return 0;
    }
#endif
}


/* -----------------------------------------------------------------------------
 * _ITM_ interface
 * -------------------------------------------------------------------------- */

uint32_t
_ITM_beginTransaction(uint32_t properties,...)
{
    /* Return values to run
     *  - uninstrumented code : 0x02
     *  - instrumented code   : 0x01
     */
    return 0x01;
}

void _ITM_commitTransaction() {}


inline void*
_ITM_malloc(size_t size)
{
    return asco_malloc(__asco, size);
}

inline void
_ITM_free(void* ptr)
{
    asco_free(__asco, ptr);
}

inline void*
_ITM_calloc(size_t nmemb, size_t size)
{
    return asco_malloc(__asco, nmemb*size);
}
#ifndef COWBACK
#define ITM_READ(type,  suffix) inline                  \
    type _ITM_R##suffix(const type* addr)               \
    {                                                   \
        if (ignore_addr(addr)) return *addr;            \
        else return asco_read_##type(__asco, addr);     \
    }
#else
#define ITM_READ(type,  suffix) inline                  \
    type _ITM_R##suffix(const type* addr)               \
    {                                                   \
        return *addr;                                   \
    }
#endif
ITM_READ(uint8_t,  U1)
ITM_READ(uint16_t, U2)
ITM_READ(uint32_t, U4)
ITM_READ(uint64_t, U8)

#define ITM_WRITE(type, suffix) inline                  \
    void _ITM_W##suffix(type* addr, type value)         \
    {                                                   \
        if (ignore_addr(addr)) *addr = value;           \
        else asco_write_##type(__asco, addr, value);    \
    }

ITM_WRITE(uint8_t,  U1)
ITM_WRITE(uint16_t, U2)
ITM_WRITE(uint32_t, U4)
ITM_WRITE(uint64_t, U8)

void
_ITM_changeTransactionMode(int flag)
{
    DLOG3("changeTransactionMode\n");
}

void*
_ITM_getTMCloneOrIrrevocable(void* ptr)
{
    DLOG3("getTMCloneOrIrrevocable\n");
    return ptr;
}


void*
_ITM_memcpyRtWt(void* dst, const void* src, size_t size)
{
    char* destination = (char*) dst;
    char* source      = (char*) src;
    uint32_t i = 0;

    do {
        //destination[i] = source[i];
        asco_write_uint8_t(__asco, (void*) (destination + i),
                           asco_read_uint8_t(__asco, (void*) (source + i)));
    } while (i++ < size);

    return (void*) destination;
}

void*
_ITM_memmoveRtWt(void* dst, const void* src, size_t size)
{
    assert(0 && "not supported yet");
    return _ITM_memcpyRtWt(dst, src, size);
}

void*
_ITM_memsetW(void* s, int c, size_t n)
{
    char* ptr = (char*) s;
    uint32_t i = 0;

    for (i = 0; i < n; ++i)
        asco_write_uint8_t(__asco, (void*) (ptr + i), c);

    return s;
}
int _ITM_initializeProcess() { return 0; }

/* -----------------------------------------------------------------------------
 * tmasco user methods
 * -------------------------------------------------------------------------- */

void*
tmasco_malloc(size_t size)
{
    assert (asco_getp(__asco) == -1 && "called from transactional code");
    return asco_malloc2(__asco, size);
}

void*
tanger_txnal_tmasco_malloc(size_t size)
{
    assert (asco_getp(__asco) != -1 && "called from non-transactional code");
    return asco_malloc(__asco, size);
}

void*
tmasco_other(void* ptr)
{
    int p = asco_getp(__asco);
    asco_setp(__asco, 0);
    void* r = asco_other(__asco, ptr);
    asco_setp(__asco, p);
    return r;
}

/* -----------------------------------------------------------------------------
 * clang-tm methods
 * -------------------------------------------------------------------------- */
void tanger_stm_indirect_init_multiple(uint32_t number_of_call_targets,
                                       uint32_t versions){}
void tanger_stm_indirect_register_multiple(void* nontxnal, void* txnal,
                                           uint32_t version){}

/* once a transaction starts, this methods gives the hint of where the
 * stack starts and ends (at the moment). Any memory between getsp()
 * and high_addr are inside the stack and shouldn't be considered by
 * asco. Any access outside that range is considered to be state of
 * the application. */
void
tanger_stm_save_restore_stack(void* low_addr, void* high_addr)
{
    __asco_low = (uintptr_t) low_addr;
    __asco_high = (uintptr_t) high_addr;
    DLOG2("low = %p, high = %p \n", __asco_low, __asco_high);
}

/* whenever a function cannot be instrumented in compile time, this
 * function is called to resolve that. We simply return the same
 * function pointer. */
void*
tanger_stm_indirect_resolve_multiple(void *nontxnal_function, uint32_t version)
{
    if (nontxnal_function == tmasco_malloc)
        return tanger_txnal_tmasco_malloc;
    else
        return nontxnal_function;
}

void*
tanger_stm_realloc(void* ptr, size_t size)
{
    assert (ptr == NULL && "not supported");
    return asco_malloc(__asco, size);
}

#endif /* TMASCO_ENABLED */
