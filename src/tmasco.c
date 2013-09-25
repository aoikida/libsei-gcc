/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <asco.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "heap.h"
#include "cow.h"

#ifndef TMASCO_ENABLED
#include "tmasco_instr.c"
#else /* TMASCO_ENABLED */

/* -----------------------------------------------------------------------------
 * tmasco state (asco object and stack boundaries)
 * -------------------------------------------------------------------------- */

typedef struct {
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t ret;
} tmasco_ctx_t;

struct {
    asco_t* asco;
    uintptr_t low;
    uintptr_t high;
    tmasco_ctx_t ctx;
} __tmasco;


#ifdef HEAP_PROTECT
void asco_unprotect(asco_t* asco, void* addr, size_t size);
#  include "protect.c"
#  define HEAP_PROTECT_INIT protect_setsignal()
PROTECT_HANDLER

#else
#  define HEAP_PROTECT_INIT
#endif

/* initialize library and allocate an asco object. This is called once
 * the library is loaded. If static liked should be called on
 * initialization of the program. */
static void __attribute__((constructor))
tmasco_init()
{
    assert (__tmasco.asco == NULL);
    __tmasco.asco = asco_init();
    assert (__tmasco.asco);
    HEAP_PROTECT_INIT;
}

static void __attribute__((destructor))
tmasco_fini()
{
    assert (__tmasco.asco);
    asco_fini(__tmasco.asco);
}

static inline uintptr_t getsp() __attribute__((always_inline));
static inline uintptr_t getsp() {
    register const uintptr_t rsp asm ("rsp");
#ifndef NDEBUG
    __tmasco.low = rsp; // this update is only necessary for debugging
#endif
    return rsp;
}

/* check whether address x is in the stack or not. */
#define IN_STACK(x) getsp() <= (uintptr_t) x && (uintptr_t) x < __tmasco.high

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
 * prototypes
 * -------------------------------------------------------------------------- */

void inline tmasco_commit();
void tmasco_switch();

/* -----------------------------------------------------------------------------
 * _ITM_ interface
 * -------------------------------------------------------------------------- */

#ifndef TMASCO_ASM
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
#else

void
_ITM_commitTransaction()
{
    tmasco_commit();
}
#endif




inline void*
_ITM_malloc(size_t size)
{
    return asco_malloc(__tmasco.asco, size);
}

inline void
_ITM_free(void* ptr)
{
    asco_free(__tmasco.asco, ptr);
}

inline void*
_ITM_calloc(size_t nmemb, size_t size)
{
    return asco_malloc(__tmasco.asco, nmemb*size);
}
#ifndef COWBACK
#define ITM_READ(type, prefix, suffix) inline                   \
    type _ITM_R##prefix##suffix(const type* addr)               \
    {                                                           \
        if (ignore_addr(addr)) return *addr;                    \
        else return asco_read_##type(__tmasco.asco, addr);      \
    }
#else
#ifdef COW_ASMREAD
#  define ITM_READ(type, prefix, suffix) inline                 \
    type _ITM_R##prefix##suffix(const type* addr);
#else
#  define ITM_READ(type, prefix, suffix) inline                 \
    type _ITM_R##prefix##suffix(const type* addr)               \
    {                                                           \
        return *addr;                                           \
    }
#endif /* COW_ASMREAD */
#endif

#define ITM_READ_ALL(type, suffix)                              \
    ITM_READ(type,   , suffix)                                  \
    ITM_READ(type, aR, suffix)                                  \
    ITM_READ(type, aW, suffix)                                  \
    ITM_READ(type, fW, suffix)

ITM_READ_ALL(uint8_t,  U1)
ITM_READ_ALL(uint16_t, U2)
ITM_READ_ALL(uint32_t, U4)
ITM_READ_ALL(uint64_t, U8)

#define ITM_WRITE(type, prefix, suffix) inline                 \
    void _ITM_W##prefix##suffix(type* addr, type value)        \
    {                                                          \
        if (ignore_addr(addr)) *addr = value;                  \
        else asco_write_##type(__tmasco.asco, addr, value);    \
    }

#define ITM_WRITE_ALL(type, suffix)                            \
    ITM_WRITE(type,   , suffix)                                \
    ITM_WRITE(type, aR, suffix)                                \
    ITM_WRITE(type, aW, suffix)

ITM_WRITE_ALL(uint8_t,  U1)
ITM_WRITE_ALL(uint16_t, U2)
ITM_WRITE_ALL(uint32_t, U4)
ITM_WRITE_ALL(uint64_t, U8)

// if x86_64
typedef union { __uint128_t sse; uint64_t v[2];} m128;
void
_ITM_WM128(void* txn, __uint128_t* a, __uint128_t v)
{
    m128 x;
    uintptr_t y;

    x.sse = v;
    y = (uintptr_t) a;

    _ITM_WU8((uint64_t*) y, x.v[0]);
    _ITM_WU8((uint64_t*)(y+sizeof(uint64_t)), x.v[1]);

}

__uint128_t
_ITM_RM128(void* txn, __uint128_t* a)
{
    m128 x;
    uint64_t* y;

    y = (uint64_t*) a;

    x.v[0] = _ITM_RU8(y);
    x.v[1] = _ITM_RU8(y+1);

    return x.sse;
}

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
#ifdef COWBACK
        asco_write_uint8_t(__tmasco.asco, (void*) (destination + i),
                           *(uint8_t*) (source + i));
#else
        asco_write_uint8_t(__tmasco.asco, (void*) (destination + i),
                           asco_read_uint8_t(__tmasco.asco,
                                             (void*) (source + i)));
#endif
    } while (i++ < size);

    return (void*) destination;
}

void*
_ZGTt6memcpy(void* dst, const void* src, size_t size)
{
    return _ITM_memcpyRtWt(dst, src, size);
}

void*
_ITM_memmoveRtWt(void* dst, const void* src, size_t size)
{
    assert(0 && "not supported yet");
    return NULL;
}

void*
_ZGTt7realloc(void* ptr, size_t size)
{
    void* p = _ITM_malloc(size);
    if (p && ptr) _ITM_memcpyRtWt(p, ptr, size);
    if (ptr) _ITM_free(ptr);
    return p;
}

void*
_ITM_memsetW(void* s, int c, size_t n)
{
    uintptr_t p    = (uintptr_t) s;
    uintptr_t p64  = p & ~(0x07);
    uintptr_t e    = p + n;
    uintptr_t e64  = e & ~(0x07);
    uint64_t  b    = (uint64_t) (char) c;
    uint64_t  v    = b << 56 | b << 48 | b << 40 | b << 32 \
        | b << 24 | b << 16 | b << 8 | b;

    if (p64 < p) p64 += 0x08;

    while (p < e && p != p64)
        asco_write_uint8_t(__tmasco.asco, (void*) (p++), c);

    while (p < e64) {
        asco_write_uint64_t(__tmasco.asco, (void*) (p), v);
        p += 8;
    }

    while (p < e)
        asco_write_uint8_t(__tmasco.asco, (void*) (p++), c);

    return s;
}

void*
_ZGTt6memset(void* s, int c, size_t n)
{
    return _ITM_memsetW(s,c,n);
}
int _ITM_initializeProcess() { return 0; }

/* -----------------------------------------------------------------------------
 * tmasco interface methods
 * -------------------------------------------------------------------------- */

void*
tmasco_malloc(size_t size)
{
    assert (asco_getp(__tmasco.asco) == -1
            && "called from transactional code");
    return asco_malloc2(__tmasco.asco, size);
}

void*
tanger_txnal_tmasco_malloc(size_t size)
{
    assert (asco_getp(__tmasco.asco) != -1
            && "called from non-transactional code");
    return asco_malloc(__tmasco.asco, size);
}

void*
tmasco_other(void* ptr)
{
    int p = asco_getp(__tmasco.asco);
    asco_setp(__tmasco.asco, 0);
    void* r = asco_other(__tmasco.asco, ptr);
    asco_setp(__tmasco.asco, p);
    return r;
}

#ifndef TMASCO_ASM
void
tmasco_begin(uintptr_t bp)
{
    __tmasco.high = bp;
    asco_begin(__tmasco.asco);
}


int
tmasco_switched()
{
    return asco_getp(__tmasco.asco);
}

void
tmasco_switch()
{
    asco_switch(__tmasco.asco);
}

void
tmasco_commit()
{
    asco_commit(__tmasco.asco);
}

#else /* TMASCO_ASM */

uint32_t
tmasco_begin(tmasco_ctx_t* ctx)
{
    memcpy(&__tmasco.ctx, ctx, sizeof(tmasco_ctx_t));
    __tmasco.high = __tmasco.ctx.rbp;
    asco_begin(__tmasco.asco);
    return 0x01;
}

void
tmasco_commit()
{
    if (!asco_getp(__tmasco.asco)) {
        asco_switch(__tmasco.asco);
        tmasco_switch(&__tmasco.ctx, 0x01);
    }
    asco_commit(__tmasco.asco);
}
#endif

int
tmasco_prepare(const void* ptr, size_t size, uint32_t crc, int ro)
{
    return asco_prepare(__tmasco.asco, ptr, size, crc, ro);
}

void
tmasco_prepare_nm(const void* ptr, size_t size, uint32_t crc, int ro)
{
    asco_prepare_nm(__tmasco.asco);
}

void
tmasco_output_append(const void* ptr, size_t size)
{
    asco_output_append(__tmasco.asco, ptr, size);
}

void
tmasco_output_done()
{
    asco_output_done(__tmasco.asco);
}

uint32_t
tmasco_output_next()
{
    return asco_output_next(__tmasco.asco);
}

void
tmasco_unprotect(void* addr, size_t size)
{
#ifdef HEAP_PROTECT
    asco_unprotect(__tmasco.asco, addr, size);
#endif
    // else ignore
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
    __tmasco.low = (uintptr_t) low_addr;
    __tmasco.high = (uintptr_t) high_addr;
    DLOG2("low = %p, high = %p \n", __tmasco.low, __tmasco.high);
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
    return asco_malloc(__tmasco.asco, size);
}

#endif /* TMASCO_ENABLED */
