/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <asco.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "heap.h"
#include "cow.h"
#include "tmasco_mt.h"

#ifndef TMASCO_ENABLED
#include "tmasco_instr.c"
#else /* TMASCO_ENABLED */

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

/* ----------------------------------------------------------------------------
 * tmasco data structures
 * ------------------------------------------------------------------------- */

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

typedef struct {
#ifdef ASCO_MT
    char pad1[64];
#endif
    asco_t* asco;
    uintptr_t low;
    uintptr_t high;
    tmasco_ctx_t ctx;
#ifdef ASCO_MT
    abuf_t* abuf;
    int wrapped;

#ifdef ASCO_MTL
    int mtl;
    uint64_t rbp;
    uint64_t rsp;
    size_t size;
#define ASCO_MAX_STACKSZ 4096*10
    char stack[ASCO_MAX_STACKSZ];
#endif  /* ASCO_MTL */

#ifdef ASCO_2PL
    abuf_t* abuf_2pl;
#endif /* ASCO_2PL */

#ifdef ASCO_TBAR
    tbar_t* tbar;
    stash_t* stash;
#endif /* ASCO_TBAR */

    char pad2[64];
#endif /* !ASCO_MT */

} tmasco_t;

/* ----------------------------------------------------------------------------
 * prototypes
 * ------------------------------------------------------------------------- */

#ifdef ASCO_MTL
void inline tmasco_commit(int);
void tmasco_switch2();
#else
void inline tmasco_commit();
#endif

void tmasco_switch();

/* ----------------------------------------------------------------------------
 * tmasco state
 * ------------------------------------------------------------------------- */

#ifndef ASCO_MT
/* __tmasco is a pointer to the state of the thread. ___tmasco is the
 * data structure itself. Hopefully the compiler is clever enough to
 * simplify the accesses via pointer to the static data structure.
 */
static tmasco_t ___tmasco;
static tmasco_t* __tmasco = &___tmasco;

#else /* ASCO_MT */
/* In Multi-Thread mode we have the tmasco data structure allocated
 * for each thread and the threads use again the __tmasco pointer to
 * access their own entry.
 */
static tmasco_t ___tmasco[ASCO_MAX_THREADS];
static pthread_mutex_t __tmasco_lock = PTHREAD_MUTEX_INITIALIZER;
static int __tmasco_thread_count = 0;
static __thread tmasco_t* __tmasco = NULL;

static pthread_mutex_lock_f*    __pthread_mutex_lock    = NULL;
static pthread_mutex_trylock_f* __pthread_mutex_trylock = NULL;
static pthread_mutex_unlock_f*  __pthread_mutex_unlock  = NULL;
static void* __pthread_handle = NULL;

#ifdef ASCO_TBAR
static tbar_t* __tbar = NULL;
#endif /* ASCO_TBAR */
#endif /* ASCO_MT */

/* ----------------------------------------------------------------------------
 * Heap protection
 * ------------------------------------------------------------------------- */

#ifdef HEAP_PROTECT
void asco_unprotect(asco_t* asco, void* addr, size_t size);
#  include "protect.c"
#  define HEAP_PROTECT_INIT protect_setsignal()
PROTECT_HANDLER

#else
#  define HEAP_PROTECT_INIT
#endif /* HEAP_PROTECT */


/* ----------------------------------------------------------------------------
 * init and fini
 * ------------------------------------------------------------------------- */

/* initialize library and allocate an asco object. This is called once
 * the library is loaded. If static liked should be called on
 * initialization of the program. */
static void __attribute__((constructor))
tmasco_init()
{
#ifndef ASCO_MT
    assert (__tmasco->asco == NULL);
    __tmasco->asco = asco_init();
    assert (__tmasco->asco);
    HEAP_PROTECT_INIT;
#else
    printf("Wrapping libpthread\n");
    __pthread_handle = dlopen("libpthread.so.0", RTLD_NOW);
    if (!__pthread_handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    __pthread_mutex_lock = (pthread_mutex_lock_f*)
        dlsym(__pthread_handle, "pthread_mutex_lock");
    if (!__pthread_mutex_lock) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    __pthread_mutex_trylock = (pthread_mutex_trylock_f*)
        dlsym(__pthread_handle, "pthread_mutex_trylock");
    if (!__pthread_mutex_trylock) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    __pthread_mutex_unlock = (pthread_mutex_unlock_f*)
        dlsym(__pthread_handle, "pthread_mutex_unlock");
    if (!__pthread_mutex_unlock) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

#ifdef ASCO_TBAR
    // create a global TBAR
    __tbar = tbar_init(ASCO_MAX_THREADS, NULL);
#endif /* ASCO_TBAR */

    int i;
    for (i = 0; i < __tmasco_thread_count; ++i) {
        ___tmasco[i].abuf = NULL;
#ifdef ASCO_MTL
        ___tmasco[i].mtl = 0;
#endif
#ifdef ASCO_2PL
        ___tmasco[i].abuf_2pl = NULL;
#endif
    }
#endif
}

#ifdef ASCO_MT
static void
tmasco_thread_init()
{
    fprintf(stderr, "initializing tmasco thread\n");
#ifndef NDEBUG
    int r =
#endif
        __pthread_mutex_lock(&__tmasco_lock);
    assert (r == 0 && "error acquiring lock");
    int me = __tmasco_thread_count++;
#ifndef NDEBUG
    r =
#endif
        __pthread_mutex_unlock(&__tmasco_lock);

    // should set after unlock other unlock fails
    __tmasco = &___tmasco[me];
    assert (r == 0 && "error releasing lock");

    assert (__tmasco->asco == NULL);
    __tmasco->asco = asco_init();
    assert (__tmasco->asco);
    __tmasco->abuf = abuf_init(100);
    __tmasco->wrapped = 0;
#ifdef ASCO_2PL
    __tmasco->abuf_2pl = abuf_init(100);
#endif /* ASCO_2PL */

#ifdef ASCO_TBAR
    __tmasco->tbar  = tbar_init(ASCO_MAX_THREADS, __tbar);
    __tmasco->stash = stash_init();
#endif /* ASCO_TBAR */
}
#endif

static void __attribute__((destructor))
tmasco_fini()
{
#ifndef ASCO_MT
    assert (__tmasco->asco);
    asco_fini(__tmasco->asco);
#else /* ASCO_MT */
    int i;
    for (i = 0; i < __tmasco_thread_count; ++i) {
        assert (___tmasco[i].asco);
        if (___tmasco[i].abuf)
            abuf_fini(___tmasco[i].abuf);
#ifdef ASCO_2PL
        if (___tmasco[i].abuf_2pl)
            abuf_fini(___tmasco[i].abuf_2pl);
#endif /* ASCO_2PL */

#ifdef ASCO_TBAR
        if (___tmasco[i].stash && stash_size(___tmasco[i].stash)) {
            int j;
            for (j = 0; j < stash_size(___tmasco[i].stash); ++j) {
                tbar_fini((tbar_t*) stash_get(___tmasco[i].stash, j));
            }
        } else {
            if (___tmasco[i].tbar)
                tbar_fini(___tmasco[i].tbar);
        }
#endif /* ASCO_TBAR */
        asco_fini(___tmasco[i].asco);
    }

#ifdef ASCO_TBAR
    tbar_fini(__tbar);
#endif /* ASCO_TBAR */

#endif /* ASCO_MT */
}

/* ----------------------------------------------------------------------------
 * stack boundaries helpers
 * ------------------------------------------------------------------------- */

static inline uintptr_t getsp() __attribute__((always_inline));
static inline uintptr_t
getsp()
{
    register const uintptr_t rsp asm ("rsp");
#ifndef NDEBUG
    __tmasco->low = rsp; // this update is only necessary for debugging
#endif
    return rsp;
}

static inline uintptr_t getbp() __attribute__((always_inline));
static inline uintptr_t
getbp()
{
    register const uintptr_t rbp asm ("rbp");
    return rbp;
}

/* check whether address x is in the stack or not. */
#define IN_STACK(x) getsp() <= (uintptr_t) x && (uintptr_t) x < __tmasco->high

#if 0
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

/* ----------------------------------------------------------------------------
 * _ITM_ interface
 * ------------------------------------------------------------------------- */

void
_ITM_commitTransaction()
{
#ifdef ASCO_MTL
    tmasco_commit(0);
#else
    tmasco_commit();
#endif /* ASCO_MTL */
}

inline void*
_ITM_malloc(size_t size)
{
    return asco_malloc(__tmasco->asco, size);
}

inline void
_ITM_free(void* ptr)
{
    asco_free(__tmasco->asco, ptr);
}

inline void*
_ITM_calloc(size_t nmemb, size_t size)
{
    return asco_malloc(__tmasco->asco, nmemb*size);
}
#ifndef COW_WT
#define ITM_READ(type, prefix, suffix) inline                   \
    type _ITM_R##prefix##suffix(const type* addr)               \
    {                                                           \
        if (ignore_addr(addr)) return *addr;                    \
        else return asco_read_##type(__tmasco->asco, addr);     \
    }
#else
#ifdef COW_ASMREAD
#  define ITM_READ(type, prefix, suffix) inline         \
    type _ITM_R##prefix##suffix(const type* addr);
#else
#  define ITM_READ(type, prefix, suffix) inline         \
    type _ITM_R##prefix##suffix(const type* addr)       \
    {                                                   \
        return *addr;                                   \
    }
#endif /* COW_ASMREAD */
#endif /* COW_WT */

#define ITM_READ_ALL(type, suffix)              \
    ITM_READ(type,   , suffix)                  \
    ITM_READ(type, aR, suffix)                  \
    ITM_READ(type, aW, suffix)                  \
    ITM_READ(type, fW, suffix)

ITM_READ_ALL(uint8_t,  U1)
ITM_READ_ALL(uint16_t, U2)
ITM_READ_ALL(uint32_t, U4)
ITM_READ_ALL(uint64_t, U8)

#define ITM_WRITE(type, prefix, suffix) inline                  \
    void _ITM_W##prefix##suffix(type* addr, type value)         \
    {                                                           \
        if (ignore_addr(addr)) *addr = value;                   \
        else {                                                  \
            DLOG3(                                              \
                "write %16p %16p size = %lu (thread = %p)\n",   \
                addr,                                           \
                (void*)(((uintptr_t)addr >> 3) << 3),           \
                sizeof(type),                                   \
                ((void*)(uintptr_t) pthread_self())             \
                );                                              \
            asco_write_##type(__tmasco->asco, addr, value);     \
        }                                                       \
    }

#define ITM_WRITE_ALL(type, suffix)             \
    ITM_WRITE(type,   , suffix)                 \
    ITM_WRITE(type, aR, suffix)                 \
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
    assert (0 && "should never change mode!");
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
#ifdef COW_WT
        asco_write_uint8_t(__tmasco->asco, (void*) (destination + i),
                           *(uint8_t*) (source + i));
#else
        asco_write_uint8_t(__tmasco->asco, (void*) (destination + i),
                           asco_read_uint8_t(__tmasco->asco,
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
        asco_write_uint8_t(__tmasco->asco, (void*) (p++), c);

    while (p < e64) {
        asco_write_uint64_t(__tmasco->asco, (void*) (p), v);
        p += 8;
    }

    while (p < e)
        asco_write_uint8_t(__tmasco->asco, (void*) (p++), c);

    return s;
}

void*
_ZGTt6memset(void* s, int c, size_t n)
{
    return _ITM_memsetW(s,c,n);
}
int _ITM_initializeProcess() { return 0; }


/* ----------------------------------------------------------------------------
 * pthread wrappers
 * ------------------------------------------------------------------------- */
#ifdef ASCO_MT

#ifdef ASCO_MTL
uint32_t _ITM_beginTransaction(uint32_t properties,...);

void
tmasco_mtl(uint64_t bp)
{
    if (! __tmasco->mtl) {
        __tmasco->mtl = 1;
        // save initial rbp
        __tmasco->rbp = __tmasco->ctx.rbp;
    }
    // copy stack
    __tmasco->rsp  = getsp();
    /* add one pointer size to stack size so that current rsp is also
       copied. Not really necessary though. */
    __tmasco->size = __tmasco->rbp - __tmasco->rsp + sizeof(uintptr_t);
    //__tmasco->size = __tmasco->size > 400 ? 400 : __tmasco->size;
    assert (__tmasco->size < ASCO_MAX_STACKSZ);
    memcpy(__tmasco->stack, (void*) (__tmasco->rsp), __tmasco->size);
    //DLOG3("STACK SIZE: %lu bytes (thread = %p)\n",
    //      __tmasco->size, (void*) pthread_self());

    // reset message
    asco_prepare_nm(__tmasco->asco);

    _ITM_beginTransaction(0);
}
#endif /* ASCO_MTL */

int
pthread_mutex_lock(pthread_mutex_t* lock)
{
    if (unlikely(!__tmasco)) { // || __tmasco->wrapped)) {
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_lock(lock);
    }
    //__tmasco->wrapped = 1;
    int r;

    switch (asco_getp(__tmasco->asco)) {
#ifdef ASCO_MTL2
    case 0:
    case 1:
        tmasco_commit(1);
        r =  __pthread_mutex_lock(lock);
        tmasco_mtl(getbp());
        break;

#else /* ASCO_MTL2 */
    case 0:
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
        abuf_push_uint64_t(__tmasco->abuf, (uint64_t*) lock, r);
#ifdef ASCO_2PL
        abuf_push_uint64_t(__tmasco->abuf_2pl, (uint64_t*) lock, r);
#endif /* ASCO_2PL */
        break;
    case 1:
        DLOG3("fake locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__tmasco->abuf, (uint64_t*) lock);
        break;
#endif /* ASCO_MTL2 */
    default:
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
        break;
    }

    //__tmasco->wrapped = 0;
    return r;
}

int
pthread_mutex_trylock(pthread_mutex_t* lock)
{
    if (unlikely(!__tmasco)) { // || __tmasco->wrapped)) {
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_trylock(lock);
    }
    //__tmasco->wrapped = 1;

    int r;
    switch (asco_getp(__tmasco->asco)) {

#ifdef ASCO_MTL2
    case 0:
    case 1:
        tmasco_commit(1);
        r =  __pthread_mutex_trylock(lock);
        tmasco_mtl(getbp());
        break;
#else
    case 0:
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
        abuf_push_uint64_t(__tmasco->abuf, (uint64_t*) lock, r);
#ifdef ASCO_2PL
        abuf_push_uint64_t(__tmasco->abuf_2pl, (uint64_t*) lock, r);
#endif /* ASCO_2PL */
        break;
    case 1:
        DLOG3("fake trylock %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__tmasco->abuf, (uint64_t*) lock);
        break;
#endif /* ASCO_MTL2 */
    default:
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
        break;
    }

    //__tmasco->wrapped = 0;
    return r;
}

#ifdef ASCO_MTL
int
pthread_mutex_unlock(pthread_mutex_t* lock)
{
    if (unlikely(!__tmasco)) { // || __tmasco_wrapped)) {
       return __pthread_mutex_unlock(lock);
    }
    //__tmasco->wrapped = 1;

    int r;
    switch (asco_getp(__tmasco->asco)) {
    case 0:
    case 1:
        tmasco_commit(1);
        DLOG3( "unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r =  __pthread_mutex_unlock(lock);
        DLOG3( "start mini traversal (thread = %p)\n", (void*) pthread_self());
        tmasco_mtl(getbp());
        break;
    default:
        r = __pthread_mutex_unlock(lock);
    }

    //__tmasco->wrapped = 0;
    return r;
}

#else /* !ASCO_MTL */
int
pthread_mutex_unlock(pthread_mutex_t* lock)
{
    if (unlikely(!__tmasco)) { // || __tmasco->wrapped)) {
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_unlock(lock);
    }
    //__tmasco->wrapped = 1;

    int r;
    switch (asco_getp(__tmasco->asco)) {
#ifndef ASCO_2PL
    case 0:
        r = __pthread_mutex_unlock(lock);
        abuf_push_uint64_t(__tmasco->abuf, (uint64_t*) lock, r);
        break;
    case 1:
        r = abuf_pop_uint64_t(__tmasco->abuf, (uint64_t*) lock);
        break;
#else /* ASCO_2PL */
    case 0:
    case 1:
        r = 0; // 0 for successful unlock
        break;
#endif /* ASCO_2PL */
    default:
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_unlock(lock);
        break;
    }

    //__tmasco->wrapped = 0;
    return r;
}
#endif /* ASCO_MTL */
#endif /* ASCO_MT */


/* ----------------------------------------------------------------------------
 * tmasco interface methods
 * ------------------------------------------------------------------------- */

void*
tmasco_malloc(size_t size)
{
    assert (asco_getp(__tmasco->asco) == -1
            && "called from transactional code");
    return asco_malloc2(__tmasco->asco, size);
}

void*
tanger_txnal_tmasco_malloc(size_t size)
{
    assert (asco_getp(__tmasco->asco) != -1
            && "called from non-transactional code");
    return asco_malloc(__tmasco->asco, size);
}

void*
tmasco_other(void* ptr)
{
    int p = asco_getp(__tmasco->asco);
    asco_setp(__tmasco->asco, 0);
    void* r = asco_other(__tmasco->asco, ptr);
    asco_setp(__tmasco->asco, p);
    return r;
}

uint32_t
tmasco_begin(tmasco_ctx_t* ctx)
{
#ifdef ASCO_MT
    assert (__tmasco && "tmasco_prepare should be called before begin");
#ifdef ASCO_TBAR
    tbar_enter(__tmasco->tbar);
#endif /* ASCO_TBAR */
#endif /* ASCO_MT */
    memcpy(&__tmasco->ctx, ctx, sizeof(tmasco_ctx_t));
    __tmasco->high = __tmasco->ctx.rbp;
    asco_begin(__tmasco->asco);
    return 0x01;
}

#ifdef ASCO_MTL
void
tmasco_commit(int force)
{
    if (!asco_getp(__tmasco->asco)) {
        asco_switch(__tmasco->asco);
        //fprintf(stderr, "Acquired locks: %d\n", abuf_size(__tmasco->abuf));

        if (__tmasco->mtl) {
            // copy stack back
            tmasco_switch2((void*)__tmasco->rsp, __tmasco->stack,
                           __tmasco->size, &__tmasco->ctx, 0x01);
        } else {
            tmasco_switch(&__tmasco->ctx, 0x01);
        }
    }
    asco_commit(__tmasco->asco);

    assert (abuf_size(__tmasco->abuf) == 0);
    abuf_clean(__tmasco->abuf);

    if (!force) {
        __tmasco->mtl = 0;
        DLOG3("Final commit! (thread = %p)\n", (void*) pthread_self());
    }
}
#else /* ! ASCO_MTL */
void
tmasco_commit()
{
    if (!asco_getp(__tmasco->asco)) {
        asco_switch(__tmasco->asco);
        tmasco_switch(&__tmasco->ctx, 0x01);
    }
    asco_commit(__tmasco->asco);
#ifdef ASCO_MT
    abuf_clean(__tmasco->abuf);
#endif

#ifdef ASCO_2PL
    int r = 0;
    pthread_mutex_t* l = NULL;
    assert (asco_getp(__tmasco->asco) == -1);
    while (abuf_size(__tmasco->abuf_2pl)) {
        l = abuf_pop(__tmasco->abuf_2pl, (void*) &r);
        // we only pushed locks and trylocks, hence if r == 0, l was
        // successfully locked.
        DLOG3("late unlocking %p (thread = %p)\n", l, (void*) pthread_self());
        if (!r) {
            r = __pthread_mutex_unlock(l);
            assert (!r && "unlock failed");
        }
    }
    abuf_clean(__tmasco->abuf_2pl);
#endif

#ifdef ASCO_TBAR
    tbar_leave(__tmasco->tbar);
#endif /* ASCO_TBAR */

}
#endif /* ! ASCO_MTL */

int
tmasco_prepare(const void* ptr, size_t size, uint32_t crc, int ro)
{
#ifdef ASCO_MT
    if (unlikely(!__tmasco)) tmasco_thread_init();
#endif
    return asco_prepare(__tmasco->asco, ptr, size, crc, ro);
}

void
tmasco_prepare_nm(const void* ptr, size_t size, uint32_t crc, int ro)
{
#ifdef ASCO_MT
    if (unlikely(!__tmasco)) tmasco_thread_init();
#endif
    asco_prepare_nm(__tmasco->asco);
}

void
tmasco_output_append(const void* ptr, size_t size)
{
    asco_output_append(__tmasco->asco, ptr, size);
}

void
tmasco_output_done()
{
    asco_output_done(__tmasco->asco);
}

uint32_t
tmasco_output_next()
{
    return asco_output_next(__tmasco->asco);
}

void
tmasco_unprotect(void* addr, size_t size)
{
#ifdef HEAP_PROTECT
    asco_unprotect(__tmasco->asco, addr, size);
#endif
    // else ignore
}

/* ----------------------------------------------------------------------------
 * Traversal management
 * ------------------------------------------------------------------------- */

int
tmasco_bar()
{
#ifdef ASCO_TBAR
    return !tbar_check(__tmasco->tbar);
#else /* ASCO_TBAR */
    return 0;
#endif /* ASCO_TBAR */
}

int
tmasco_shift(int handle)
{
#ifdef ASCO_MT
    if (unlikely(!__tmasco)) tmasco_thread_init();
#endif

#ifdef ASCO_TBAR
    // assume we have correct handle already in-place
    if (handle == -1) {
        // create new obuf and exchange; use current if first time */
        if (stash_size(__tmasco->stash) != 0) {
            // here we assume that current tbar already in stash
            __tmasco->tbar = tbar_idup(__tmasco->tbar);
        }
        // add to stash
#ifndef NDEBUG
        int h =
            asco_shift(__tmasco->asco, handle);
#endif
        handle = stash_add(__tmasco->stash, __tmasco->tbar);
        //TODO: assert (handle == h);
    } else {
        // shift and exchange tbar
#ifndef NDEBUG
        int h =
#endif
            asco_shift(__tmasco->asco, handle);
        //TODO: assert (handle == h);
        __tmasco->tbar = stash_get(__tmasco->stash, handle);
        assert (__tmasco->tbar);
    }
    return handle;
#else /* ASCO_TBAR */
    return asco_shift(__tmasco->asco, handle);
#endif /* ASCO_TBAR */
}

#endif /* TMASCO_ENABLED */
