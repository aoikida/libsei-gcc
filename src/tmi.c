/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
/* tmi - transactional memory interface
 * 
 * tmi implements the transactional memory ABI and additional functions
 * exposed to the user. A sei_thread object is kept for each thread.
 * ------------------------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include "sei.h"
#include "debug.h"
#include "heap.h"
#include "cow.h"
#include "tmi_mt.h"
#include "config.h"

#ifdef SEI_WRAP_SC
#include "tmi_sc.h"
#include "wts.h"
#endif

#ifdef SEI_CPU_ISOLATION
#define _GNU_SOURCE
#include <sched.h>
#include "cpu_isolation.h"

/* Thread-local storage for phase0 core tracking (SEI_CPU_ISOLATION_MIGRATE_PHASES) */
#ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
static __thread int phase0_core = -1;
#endif
#endif

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

/* ----------------------------------------------------------------------------
 * N-way DMR Configuration
 * ------------------------------------------------------------------------- */

#ifndef SEI_DMR_REDUNDANCY
#define SEI_DMR_REDUNDANCY 2
#endif

/* ----------------------------------------------------------------------------
 * sei data structures
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
} sei_ctx_t;

typedef struct {
#ifdef SEI_MT
    char pad1[64];
#endif
    sei_t* sei;
    uintptr_t low;
    uintptr_t high;
    sei_ctx_t ctx;
#ifdef SEI_MT
    abuf_t* abuf;
    int wrapped;

#ifdef SEI_MTL
    int mtl;
    uint64_t rbp;
    uint64_t rsp;
    size_t size;
#define SEI_MAX_STACKSZ 4096*10
    char stack[SEI_MAX_STACKSZ];
#endif  /* SEI_MTL */

#ifdef SEI_2PL
    abuf_t* abuf_2pl;
#endif /* SEI_2PL */

#ifdef SEI_TBAR
    tbar_t* tbar;
    stash_t* stash;
#endif /* SEI_TBAR */

    char pad2[64];
#endif /* !SEI_MT */

#ifdef SEI_WRAP_SC
    abuf_t* abuf_sc; /* buffer for return values of wrapped system calls */
#endif

} sei_thread_t;

/* ----------------------------------------------------------------------------
 * prototypes
 * ------------------------------------------------------------------------- */

#ifdef SEI_MTL
void inline __sei_commit(int);
void __sei_switch2();
#else
void inline __sei_commit();
#endif

void __sei_switch();

/* ----------------------------------------------------------------------------
 * sei_thread state
 * ------------------------------------------------------------------------- */

#ifndef SEI_MT
/* __sei_thread is a pointer to the state of the thread. ___sei_thread is the
 * data structure itself. Hopefully the compiler is clever enough to
 * simplify the accesses via pointer to the static data structure.
 */
static sei_thread_t ___sei_thread;
static sei_thread_t* __sei_thread = &___sei_thread;

#else /* SEI_MT */
/* In Multi-Thread mode we have the sei_thread data structure allocated
 * for each thread and the threads use again the __sei_thread pointer to
 * access their own entry.
 */
static sei_thread_t ___sei_thread[SEI_MAX_THREADS];
static pthread_mutex_t __sei_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static int __sei_thread_count = 0;
static __thread sei_thread_t* __sei_thread = NULL;

static pthread_mutex_lock_f*    __pthread_mutex_lock    = NULL;
static pthread_mutex_trylock_f* __pthread_mutex_trylock = NULL;
static pthread_mutex_unlock_f*  __pthread_mutex_unlock  = NULL;
static void* __pthread_handle = NULL;

#ifdef SEI_TBAR
static tbar_t* __tbar = NULL;
#endif /* SEI_TBAR */
#endif /* SEI_MT */

#ifdef SEI_WRAP_SC
socket_f*  __socket  = NULL;
close_f*   __close   = NULL;
bind_f*    __bind    = NULL;
connect_f* __connect = NULL;
send_f*    __send    = NULL;
sendto_f*  __sendto  = NULL;
#endif

/* ----------------------------------------------------------------------------
 * CRC redundancy configuration (compile-time)
 * ------------------------------------------------------------------------- */
#ifndef SEI_CRC_REDUNDANCY
#define SEI_CRC_REDUNDANCY 2
#endif

#if SEI_CRC_REDUNDANCY < 2 || SEI_CRC_REDUNDANCY > 10
#error "SEI_CRC_REDUNDANCY must be between 2 and 10"
#endif

/* ----------------------------------------------------------------------------
 * Heap protection
 * ------------------------------------------------------------------------- */

#ifdef HEAP_PROTECT
void sei_unprotect(sei_t* sei, void* addr, size_t size);
#  include "protect.c"
#  define HEAP_PROTECT_INIT protect_setsignal()
PROTECT_HANDLER

#else
#  define HEAP_PROTECT_INIT
#endif /* HEAP_PROTECT */

#ifdef SEI_WRAP_SC
#define SYSCALL_WRAPPER_INIT(name)                                      \
    do {                                                                \
        __##name = (name##_f*) dlsym(RTLD_NEXT, #name);                 \
            if (!__##name) {                                            \
                fprintf(stderr, "%s\n", dlerror());                     \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
    }                                                                   \
    while(0)
#endif

/* ----------------------------------------------------------------------------
 * init and fini
 * ------------------------------------------------------------------------- */

/* initialize library and allocate an sei object. This is called once
 * the library is loaded. If static liked should be called on
 * initialization of the program. */
static void __attribute__((constructor))
__sei_init()
{
#ifdef SEI_CPU_ISOLATION
    /* Initialize CPU Isolation Manager */
    if (cpu_isolation_init() != 0) {
        fprintf(stderr, "[libsei] Failed to initialize CPU isolation\n");
        exit(EXIT_FAILURE);
    }
#endif

#ifndef SEI_MT
    assert (__sei_thread->sei == NULL);
    __sei_thread->sei = sei_init();
    assert (__sei_thread->sei);
    HEAP_PROTECT_INIT;

#ifdef SEI_WRAP_SC
    __sei_thread->abuf_sc = abuf_init(SC_MAX_CALLS);
#endif

#else
    DLOG1("Wrapping libpthread\n");
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

#ifdef SEI_TBAR
    // create a global TBAR
    __tbar = tbar_init(SEI_MAX_THREADS, NULL);
#endif /* SEI_TBAR */

    int i;
    for (i = 0; i < __sei_thread_count; ++i) {
        ___sei_thread[i].abuf = NULL;
#ifdef SEI_MTL
        ___sei_thread[i].mtl = 0;
#endif
#ifdef SEI_2PL
        ___sei_thread[i].abuf_2pl = NULL;
#endif
    }
#endif

#ifdef SEI_WRAP_SC
    SYSCALL_WRAPPER_INIT(socket);
    SYSCALL_WRAPPER_INIT(bind);
    SYSCALL_WRAPPER_INIT(close);
    SYSCALL_WRAPPER_INIT(connect);
    SYSCALL_WRAPPER_INIT(send);
    SYSCALL_WRAPPER_INIT(sendto);
#endif /* SEI_WRAP_SC */
}

#ifdef SEI_MT
static void
__sei_thread_init()
{
    DLOG1("initializing sei thread\n");
#ifndef NDEBUG
    int r =
#endif
        __pthread_mutex_lock(&__sei_thread_lock);
    assert (r == 0 && "error acquiring lock");
    int me = __sei_thread_count++;
#ifndef NDEBUG
    r =
#endif
        __pthread_mutex_unlock(&__sei_thread_lock);

    // should set after unlock other unlock fails
    __sei_thread = &___sei_thread[me];
    assert (r == 0 && "error releasing lock");

    assert (__sei_thread->sei == NULL);
    __sei_thread->sei = sei_init();
    assert (__sei_thread->sei);
    __sei_thread->abuf = abuf_init(100);
    __sei_thread->wrapped = 0;
#ifdef SEI_2PL
    __sei_thread->abuf_2pl = abuf_init(100);
#endif /* SEI_2PL */

#ifdef SEI_TBAR
    __sei_thread->tbar  = tbar_init(SEI_MAX_THREADS, __tbar);
    __sei_thread->stash = stash_init();
#endif /* SEI_TBAR */
#ifdef SEI_WRAP_SC
    __sei_thread->abuf_sc = abuf_init(SC_MAX_CALLS);
#endif
}
#endif

static void __attribute__((destructor))
__sei_fini()
{

#ifndef SEI_MT
#ifdef SEI_WRAP_SC
    abuf_fini(__sei_thread->abuf_sc);
#endif /* SEI_WRAP_SC */

    assert (__sei_thread->sei);
    sei_fini(__sei_thread->sei);

#else /* SEI_MT */
    int i;
    for (i = 0; i < __sei_thread_count; ++i) {
        assert (___sei_thread[i].sei);
        if (___sei_thread[i].abuf)
            abuf_fini(___sei_thread[i].abuf);
#ifdef SEI_2PL
        if (___sei_thread[i].abuf_2pl)
            abuf_fini(___sei_thread[i].abuf_2pl);
#endif /* SEI_2PL */

#ifdef SEI_TBAR
        if (___sei_thread[i].stash && stash_size(___sei_thread[i].stash)) {
            int j;
            for (j = 0; j < stash_size(___sei_thread[i].stash); ++j) {
                tbar_fini((tbar_t*) stash_get(___sei_thread[i].stash, j));
            }
        } else {
            if (___sei_thread[i].tbar)
                tbar_fini(___sei_thread[i].tbar);
        }
#endif /* SEI_TBAR */
        sei_fini(___sei_thread[i].sei);
    }

#ifdef SEI_TBAR
    tbar_fini(__tbar);
#endif /* SEI_TBAR */

#endif /* SEI_MT */

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
    __sei_thread->low = rsp; // this update is only necessary for debugging
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
#define IN_STACK(x) (getsp() <= (uintptr_t) x \
                     && (uintptr_t) x < __sei_thread->high)

#if 1
#define SEI_MAX_IGNORE 1000
void* __sei_ignore_addr_s[SEI_MAX_IGNORE];
void* __sei_ignore_addr_e[SEI_MAX_IGNORE];
uint32_t __sei_ignore_num = 0;
uint32_t __sei_ignore_allf = 0;
int __sei_write_disable = 0;
#endif

void __sei_ignore(int v) {
	__sei_write_disable = v;
}

void __sei_ignore_all(uint32_t v) {
	__sei_ignore_allf = v;
}

void __sei_ignore_addr(void* start, void* end) {
	if (sei_getp(__sei_thread->sei) == -1)
		return;
	int i;
	for (i = 0; i < __sei_ignore_num; ++i) {
		if (__sei_ignore_addr_s[i] == start &&
		    __sei_ignore_addr_e[i] == end)
			return;
	}

	__sei_ignore_addr_s[__sei_ignore_num] = start;
	__sei_ignore_addr_e[__sei_ignore_num] = end;
	__sei_ignore_num++;
	assert(SEI_MAX_IGNORE >= __sei_ignore_num && "not enough ignore slots");
	DLOG3("Ignore range from %p to %p\n", start, end);
}

/* addresses inside the stack are local variables and shouldn't be
 * considered when reading and writing. Other addresses can be
 * ignored, for example, global variables.
 */
static int inline ignore_addr(const void* ptr) __attribute__((always_inline));
static int inline
ignore_addr(const void* ptr)
{
    if (__sei_write_disable || IN_STACK(ptr)) {
        DLOG3("Ignore address: %p\n", ptr);
        return 1;
    }// else return 0;

#if 1
//    if ((uintptr_t) ptr < (uintptr_t) &edata) return 1;

        int i;
        for (i = 0; i < __sei_ignore_num; ++i)
            if (ptr >= __sei_ignore_addr_s[i] && ptr <= __sei_ignore_addr_e[i]) {
            	DLOG3("(hack) Ignore address: %p from range %d \n", ptr, i);
            	return 1;
            }
        return 0;
#endif
}

/* ----------------------------------------------------------------------------
 * _ITM_ interface
 * ------------------------------------------------------------------------- */

void
_ITM_commitTransaction()
{
#ifdef SEI_MTL
    __sei_commit(0);
#else
    __sei_commit();
#endif /* SEI_MTL */
}

void*
_ITM_malloc(size_t size)
{
    if (__sei_ignore_allf) {
        void* r = malloc(size); //sei_malloc(__sei_thread->sei, size);
        __sei_ignore_addr(r, (uint8_t*)r + size);
        return r;
    } else return sei_malloc(__sei_thread->sei, size);
}

void
_ITM_free(void* ptr)
{
    int i;
    for (i = 0; i < __sei_ignore_num; ++i)
            if (ptr == __sei_ignore_addr_s[i]) {
                free(ptr);
                return;
            }
    sei_free(__sei_thread->sei, ptr);
}

void*
_ITM_calloc(size_t nmemb, size_t size)
{
    return sei_malloc(__sei_thread->sei, nmemb*size);
}
#ifndef COW_WT
#define ITM_READ(type, prefix, suffix)                         \
    type _ITM_R##prefix##suffix(const type* addr)               \
    {                                                           \
        if (ignore_addr(addr)) return *addr;                    \
        else return sei_read_##type(__sei_thread->sei, addr);     \
    }
#else
#ifdef COW_ASMREAD
#  define ITM_READ(type, prefix, suffix)                   \
    type _ITM_R##prefix##suffix(const type* addr);
#else
#  define ITM_READ(type, prefix, suffix)                 \
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

#define ITM_WRITE(type, prefix, suffix)                         \
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
            sei_write_##type(__sei_thread->sei, addr, value);     \
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

    if (ignore_addr(dst)) {
        DLOG3("_ITM_memcpyRtWt ignore stack write source %p dest %p size %u\n",
              src, dst, size);

        memcpy(dst, src, size);
        return (void*) destination;
    }
    DLOG3("Start memcpy, size %d\n", size);

#ifdef COW_WT 

#ifndef COW_APPEND_ONLY 

    if (size >= sizeof(uint32_t)) {
        uint32_t len = size;
    	uint32_t unal = (unsigned long int)destination % sizeof(uint32_t);

    	if (unal > 0) {
    		unal = sizeof(uint32_t) - unal;

			do {
				sei_write_uint8_t(__sei_thread->sei, (void*) (destination + i),
						   *(uint8_t*) (source + i));
			} while (++i < unal);
		}

    	len -= i;

    	uint32_t unal64 = (unsigned long int)(destination + i) % sizeof(uint64_t);

    	if (unal64 > 0 && len >= sizeof(uint32_t)) {
    		sei_write_uint32_t(__sei_thread->sei, (void*) (destination + i),
    							   *(uint32_t*) (source + i));

    		i += sizeof(uint32_t);
    		len -= sizeof(uint32_t);
    	}

    	uint32_t num64w = len / sizeof(uint64_t);
		uint32_t j = 0;

		while (j < num64w) {
			sei_write_uint64_t(__sei_thread->sei, (void*) (destination + i),
					   *(uint64_t*) (source + i));
			i += sizeof(uint64_t);
			len -= sizeof(uint64_t);
			j++;
		};

		if (len >= sizeof(uint32_t)) {
			sei_write_uint32_t(__sei_thread->sei, (void*) (destination + i),
							   *(uint32_t*) (source + i));

			i += sizeof(uint32_t);
		}

		while (i < size) {
			sei_write_uint8_t(__sei_thread->sei, (void*) (destination + i),
				   *(uint8_t*) (source + i));
			i++;
		}

		DLOG3("End memcpy\n");
		return (void*) destination;
    }
#endif

#endif

    do {
        //destination[i] = source[i];
#ifdef COW_WT
        sei_write_uint8_t(__sei_thread->sei, (void*) (destination + i),
                           *(uint8_t*) (source + i));
#else
        sei_write_uint8_t(__sei_thread->sei, (void*) (destination + i),
                           sei_read_uint8_t(__sei_thread->sei,
                                             (void*) (source + i)));
#endif
    } while (++i < size);


    DLOG3("End memcpy");
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
    return _ITM_memcpyRtWt(dst, src, size);

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

void
_ITM_LB (const void *ptr, size_t len) {

}

void*
_ITM_memsetW(void* s, int c, size_t n)
{
    if (ignore_addr(s)) {
        DLOG3("_ITM_memsetW ignore stack write\n");

        memset(s, c, n);
        return s;
    }

    uintptr_t p    = (uintptr_t) s;
    uintptr_t p64  = p & ~(0x07);
    uintptr_t e    = p + n;
    uintptr_t e64  = e & ~(0x07);
    uint64_t  b    = (uint64_t) (char) c;
    uint64_t  v    = b << 56 | b << 48 | b << 40 | b << 32 \
        | b << 24 | b << 16 | b << 8 | b;

    if (p64 < p) p64 += 0x08;

    while (p < e && p != p64)
        sei_write_uint8_t(__sei_thread->sei, (void*) (p++), c);

    while (p < e64) {
        sei_write_uint64_t(__sei_thread->sei, (void*) (p), v);
        p += 8;
    }

    while (p < e)
        sei_write_uint8_t(__sei_thread->sei, (void*) (p++), c);

    return s;
}

void*
_ZGTt6memset(void* s, int c, size_t n)
{
    return _ITM_memsetW(s,c,n);
}
int _ITM_initializeProcess() { return 0; }


/* ----------------------------------------------------------------------------
 * system calls wrappers
 * --------------------------------------------------------------------------*/
#ifdef SEI_WRAP_SC

int
socket(int domain, int type, int protocol)
{
    if (unlikely(!__sei_thread)) {
        return __socket(domain, type, protocol);
    }
    int r;

    switch (sei_getp(__sei_thread->sei)) {
    case 0:
        r = __socket(domain, type, protocol);
        DLOG3("calling socket, result %d (thread = %p)\n",
              r, (void*) pthread_self());
        abuf_push_uint32_t(__sei_thread->abuf_sc, (uint32_t*)(intptr_t) domain, r);
        break;
    case 1:
        r = abuf_pop_uint32_t(__sei_thread->abuf_sc, (uint32_t*)(intptr_t) domain);
        DLOG3("fake calling socket, result %d (thread = %p)\n",
              r, (void*) pthread_self());
        break;
    default:
        DLOG3("calling socket outside (thread = %p)\n",
              (void*) pthread_self());
        r = __socket(domain, type, protocol);
        break;
    }

    return r;
}

int
wts_close(uint64_t* args)
{
	int res;
	res = __close((int) args[0]);
	DLOG3("result of close %d", res);
	//assert(res == args[1] && "unexpected result of close");
	return res;
}

int
close(int fd)
{
    if (unlikely(!__sei_thread)) {
        return __close(fd);
    }
    int r;
    int p = sei_getp(__sei_thread->sei);

    switch (p) {
    case 0:
    case 1: {
    	DLOG3("got args for close: %d\n", fd);
    	wts_add(sei_get_wts(__sei_thread->sei), p, wts_close, 2, (uint64_t) fd,
    			(uint64_t) 0);
		DLOG3("deferring close (thread = %p)\n", (void*) pthread_self());
		r = 0;
        break;
    }
    default:
        DLOG3("calling close outside (thread = %p)\n", (void*) pthread_self());
        r = __close(fd);
        break;
    }

    return r;
}

int
connect(int socket, const struct sockaddr *addr, socklen_t length)
{
    if (unlikely(!__sei_thread)) {
        return __connect(socket, addr, length);
    }
    int r;

    switch (sei_getp(__sei_thread->sei)) {
    case 0:
        r = __connect(socket, addr, length);
        abuf_push_uint32_t(__sei_thread->abuf_sc, (uint32_t*)(intptr_t) socket, r);
        DLOG3("calling connect, retval %d (thread = %p)\n", r,
        	  (void*) pthread_self());
        break;
    case 1:
        r = abuf_pop_uint32_t(__sei_thread->abuf_sc, (uint32_t*)(intptr_t) socket);
        DLOG3("fake calling connect, retval %d (thread = %p)\n", r,
        	  (void*) pthread_self());
        break;
    default:
        DLOG3("calling connect outside (thread = %p)\n",
              (void*) pthread_self());
        r = __connect(socket, addr, length);
        break;
    }

    return r;
}

int
bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (unlikely(!__sei_thread)) {
        return __bind(sockfd, addr, addrlen);
    }
    int r;

    switch (sei_getp(__sei_thread->sei)) {
    case 0:
        r = __bind(sockfd, addr, addrlen);
        abuf_push_uint32_t(__sei_thread->abuf_sc, (uint32_t*) addr, r);
        DLOG3("calling bind, retval %d (thread = %p)\n", r,
        	  (void*) pthread_self());
        break;
    case 1:
        r = abuf_pop_uint32_t(__sei_thread->abuf_sc, (uint32_t*) addr);
        DLOG3("fake calling bind, retval %d (thread = %p)\n", r,
        	  (void*) pthread_self());
        break;
    default:
        DLOG3("calling bind outside (thread = %p)\n", (void*) pthread_self());
        r = __bind(sockfd, addr, addrlen);
        break;
    }

    return r;
}


int
wts_send(uint64_t* args)
{
	int res;
	assert (__send);

	DLOG1("real send args: %d %p %d %d\n", args[0], args[1], args[2], args[3]);

	res = __send((int) args[0],
				 (void*) args[1],
				 (size_t) args[2],
				 (int) args[3]);

	assert(res == args[4] && "unexpected result of send");
	return res;
}

ssize_t
send(int socket, const void *buffer, size_t size, int flags)
{
    if (unlikely(!__sei_thread)) {
        return __send(socket, buffer, size, flags);
    }
    int r;
    int p = sei_getp(__sei_thread->sei);

    switch (p) {
    case 0:
    case 1: {
    	wts_add(sei_get_wts(__sei_thread->sei), p, wts_send, 5, (uint64_t) socket,
    			(uint64_t) buffer, (uint64_t) size, (uint64_t) flags,
    			(uint64_t) size); //last arg - expected ret value

        DLOG1("deferring send (thread = %p)\n", (void*) pthread_self());
        r = size;
        break;
    }
    default:
        DLOG1("calling send outside (thread = %p)\n", (void*) pthread_self());
        r = __send(socket, buffer, size, flags);
        break;
    }

    return r;
}

int
wts_sendto(uint64_t* args)
{
	int res;
	assert (__sendto);

	if (args[7] == 0)
		res = __sendto((int) args[0],
					 (void*) args[1],
					 (size_t) args[2],
					 (int) args[3],
					 (struct sockaddr*) args[4],
					 (socklen_t) args[5]);
	else
		res = __sendto((int) args[0],
					 (void*) args[1],
					 (size_t) args[2],
					 (int) args[3],
					 (struct sockaddr*) &args[8],
					 (socklen_t) args[5]);

	DLOG1("result of sendto %d expected %d \n", res, args[6]);

	assert(res == args[6] && "unexpected result of sendto");
	return res;
}

ssize_t sendto(int socket, const void *buffer, size_t size, int flags,
                     const struct sockaddr *dest_addr, socklen_t addrlen)
{
	if (unlikely(!__sei_thread)) {
		return __sendto(socket, buffer, size, flags, dest_addr, addrlen);
	}
	int r;
	int p = sei_getp(__sei_thread->sei);

	switch (p) {
	case 0:
	case 1: {

		if (IN_STACK(dest_addr)) {
			assert(sizeof(struct sockaddr) == 16);

			uint64_t a1, a2;
			unsigned char* d = (unsigned char*) dest_addr;
			memcpy(&a1, d, 8);
			memcpy(&a2, d + 8, 8);

			wts_add(sei_get_wts(__sei_thread->sei), p, wts_sendto, 10,
								(uint64_t) socket, (uint64_t) buffer,
								(uint64_t) size, (uint64_t) flags,
								(uint64_t) dest_addr, (uint64_t) addrlen,
								(uint64_t) size, 1/*dest_addr on stack*/, a1, a2);
		}
		else
			wts_add(sei_get_wts(__sei_thread->sei), p, wts_sendto, 8,
					(uint64_t) socket, (uint64_t) buffer, (uint64_t) size,
					(uint64_t) flags, (uint64_t) dest_addr, (uint64_t) addrlen,
					(uint64_t) size, 0/*dest_addr not on stack*/);



		DLOG1("deferring sendto (thread = %p)\n", (void*) pthread_self());
		r = size;
		break;
	}
	default:
		DLOG1("calling sendto outside (thread = %p)\n", (void*) pthread_self());
		r = __sendto(socket, buffer, size, flags, dest_addr, addrlen);
		break;
	}

	return r;
}

#endif
/* ----------------------------------------------------------------------------
 * pthread wrappers
 * ------------------------------------------------------------------------- */
#ifdef SEI_MT

#ifdef SEI_MTL
uint32_t _ITM_beginTransaction(uint32_t properties,...);

void
__sei_mtl(uint64_t bp)
{
    if (! __sei_thread->mtl) {
        __sei_thread->mtl = 1;
        // save initial rbp
        __sei_thread->rbp = __sei_thread->ctx.rbp;
    }
    // copy stack
    __sei_thread->rsp  = getsp();
    /* add one pointer size to stack size so that current rsp is also
       copied. Not really necessary though. */
    __sei_thread->size = __sei_thread->rbp - __sei_thread->rsp + sizeof(uintptr_t);
    //__sei_thread->size = __sei_thread->size > 400 ? 400 : __sei_thread->size;
    assert (__sei_thread->size < SEI_MAX_STACKSZ);
    memcpy(__sei_thread->stack, (void*) (__sei_thread->rsp), __sei_thread->size);
    //DLOG3("STACK SIZE: %lu bytes (thread = %p)\n",
    //      __sei_thread->size, (void*) pthread_self());

    // reset message
    sei_prepare_nm(__sei_thread->sei);

    _ITM_beginTransaction(0);
}
#endif /* SEI_MTL */

int
pthread_mutex_lock(pthread_mutex_t* lock)
{
    if (unlikely(!__sei_thread)) { // || __sei_thread->wrapped)) {
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_lock(lock);
    }
    //__sei_thread->wrapped = 1;
    int r;

    int phase = sei_getp(__sei_thread->sei);
#ifdef SEI_MTL2
    if (phase == 0 || phase == 1) {
        __sei_commit(1);
        r =  __pthread_mutex_lock(lock);
        __sei_mtl(getbp());
    } else {
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
    }
#else /* SEI_MTL2 */
    if (phase == 0) {
        /* Phase 0: Actually acquire lock and record result */
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
    } else if (phase > 0) {
        /* Phase 1 ~ N-1: Fake lock (read from recorded result) */
        DLOG3("fake locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
    } else {
        /* phase == -1: Outside transaction */
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
    }
#endif /* SEI_MTL2 */

    //__sei_thread->wrapped = 0;
    return r;
}

int
pthread_mutex_trylock(pthread_mutex_t* lock)
{
    if (unlikely(!__sei_thread)) { // || __sei_thread->wrapped)) {
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_trylock(lock);
    }
    //__sei_thread->wrapped = 1;

    int r;
    int phase = sei_getp(__sei_thread->sei);

#ifdef SEI_MTL2
    if (phase == 0 || phase == 1) {
        __sei_commit(1);
        r =  __pthread_mutex_trylock(lock);
        __sei_mtl(getbp());
    } else {
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
    }
#else /* SEI_MTL2 */
    if (phase == 0) {
        /* Phase 0: Actually try lock and record result */
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
#ifdef SEI_2PL
        //abuf_push_uint64_t(__sei_thread->abuf_2pl, (uint64_t*) lock, r);
#endif /* SEI_2PL */
    } else if (phase > 0) {
        /* Phase 1 ~ N-1: Fake trylock (read from recorded result) */
        DLOG3("fake trylock %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
    } else {
        /* phase == -1: Outside transaction */
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
    }
#endif /* SEI_MTL2 */

    //__sei_thread->wrapped = 0;
    return r;
}

#ifdef SEI_MTL
int
pthread_mutex_unlock(pthread_mutex_t* lock)
{
    if (unlikely(!__sei_thread)) { // || __sei_thread_wrapped)) {
       return __pthread_mutex_unlock(lock);
    }
    //__sei_thread->wrapped = 1;

    int r;
    int phase = sei_getp(__sei_thread->sei);
    if (phase == 0 || phase == 1) {
        __sei_commit(1);
        DLOG3( "unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r =  __pthread_mutex_unlock(lock);
        DLOG3( "start mini traversal (thread = %p)\n", (void*) pthread_self());
        __sei_mtl(getbp());
    } else {
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_unlock(lock);
    }

    //__sei_thread->wrapped = 0;
    return r;
}

#else /* !SEI_MTL */
int
pthread_mutex_unlock(pthread_mutex_t* lock)
{
    if (unlikely(!__sei_thread)) { // || __sei_thread->wrapped)) {
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        return __pthread_mutex_unlock(lock);
    }
    //__sei_thread->wrapped = 1;

    int r;
    int phase = sei_getp(__sei_thread->sei);

#ifndef SEI_2PL
    if (phase == 0) {
        /* Phase 0: Actually unlock and record result */
        r = __pthread_mutex_unlock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
    } else if (phase > 0) {
        /* Phase 1 ~ N-1: Fake unlock (read from recorded result) */
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
    } else {
        /* phase == -1: Outside transaction */
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_unlock(lock);
    }
#else /* SEI_2PL */
    if (phase >= 0) {
        /* Phase 0 ~ N-1: Skip unlock, locks released at commit time */
        r = 0; // 0 for successful unlock
    } else {
        /* phase == -1: Outside transaction */
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_unlock(lock);
    }
#endif /* SEI_2PL */

    //__sei_thread->wrapped = 0;
    return r;
}
#endif /* SEI_MTL */
#endif /* SEI_MT */


/* ----------------------------------------------------------------------------
 * sei_thread interface methods
 * ------------------------------------------------------------------------- */

void*
__sei_malloc(size_t size)
{
    assert (sei_getp(__sei_thread->sei) == -1
            && "called from transactional code");
    return sei_malloc2(__sei_thread->sei, size);
}

void*
tanger_txnal_sei_thread_malloc(size_t size)
{
    assert (sei_getp(__sei_thread->sei) != -1
            && "called from non-transactional code");
    return sei_malloc(__sei_thread->sei, size);
}

void*
__sei_other(void* ptr)
{
    int p = sei_getp(__sei_thread->sei);
    sei_setp(__sei_thread->sei, 0);
    void* r = sei_other(__sei_thread->sei, ptr);
    sei_setp(__sei_thread->sei, p);
    return r;
}

uint32_t
__sei_begin(sei_ctx_t* ctx)
{
#ifdef SEI_MT
    assert (__sei_thread && "sei_thread_prepare should be called before begin");
#ifdef SEI_TBAR
    tbar_enter(__sei_thread->tbar);
#endif /* SEI_TBAR */
#endif /* SEI_MT */
    memcpy(&__sei_thread->ctx, ctx, sizeof(sei_ctx_t));
    __sei_thread->high = __sei_thread->ctx.rbp;
    sei_begin(__sei_thread->sei);
    return 0x01;
}

#ifdef SEI_MTL
void
__sei_commit(int force)
{
    if (!sei_getp(__sei_thread->sei)) {
        sei_switch(__sei_thread->sei);
        //fprintf(stderr, "Acquired locks: %d\n", abuf_size(__sei_thread->abuf));

        if (__sei_thread->mtl) {
            // copy stack back
            __sei_switch2((void*)__sei_thread->rsp, __sei_thread->stack,
                           __sei_thread->size, &__sei_thread->ctx, 0x01);
        } else {
            __sei_switch(&__sei_thread->ctx, 0x01);
        }
    }
    sei_commit(__sei_thread->sei);

    assert (abuf_size(__sei_thread->abuf) == 0);
    abuf_clean(__sei_thread->abuf);

    if (!force) {
        __sei_thread->mtl = 0;
        DLOG3("Final commit! (thread = %p)\n", (void*) pthread_self());
    }
}
#else /* ! SEI_MTL */
void
__sei_commit()
{
    //	memset(__sei_ignore_addr_s, 0, sizeof(__sei_ignore_addr_s));
    // 	memset(__sei_ignore_addr_e, 0, sizeof(__sei_ignore_addr_e));
#ifdef DEBUG
    if (sei_getp(__sei_thread->sei)) {
        DLOG2("__sei_ignore_num = %d\n", __sei_ignore_num);
    }
#endif
	__sei_ignore_num = 0;
    __sei_write_disable = 0;

    int current_phase = sei_getp(__sei_thread->sei);
    int redundancy_level = sei_get_redundancy(__sei_thread->sei);
    //fprintf(stderr, "[VERIFICATION] __sei_commit called: current_phase=%d, redundancy_level=%d\n",current_phase, redundancy_level);

    /* Phase 0 ~ N-2: Switch to next phase and re-execute transaction */
    if (current_phase < redundancy_level - 1) {
        DLOG2("Phase %d completed, switching to phase %d\n",
              current_phase, current_phase + 1);

#ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
        /* Record phase0 core before switching (only for phase 0→1) */
        if (current_phase == 0) {
            phase0_core = sched_getcpu();
        }
#endif

        /* Switch to next phase */
        sei_switch(__sei_thread->sei);

#ifdef SEI_MT
        /* Rewind pthread abuf for Phase 1+ to re-read recorded lock results
         * This is needed for N-way redundancy (N >= 3) where multiple phases
         * need to replay the same recorded lock operations */
        abuf_rewind(__sei_thread->abuf);
#endif

#ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
        /* Migrate to a different core for phase1 execution (only for phase 0→1)
         * Temporarily set sei->p = -1 to prevent pthread wrappers from
         * trying to record operations to abuf during migration */
        if (current_phase == 0) {
            sei_setp(__sei_thread->sei, -1);
            int old_core = phase0_core;
            int new_core = cpu_isolation_migrate_excluding_core(phase0_core);
            //fprintf(stderr, "[libsei] Phase migration: core %d (phase0) -> core %d (phase1)\n", old_core, new_core);
            /* Restore sei->p = current_phase + 1 for next phase execution */
            sei_setp(__sei_thread->sei, current_phase + 1);
        }
#endif

        /* Context switch to re-execute transaction in next phase */
        __sei_switch(&__sei_thread->ctx, 0x01);
        return;  /* Execution continues in next phase */
    }

    /* Phase N-1 (final phase): Perform N-way verification and commit */
    assert(current_phase == redundancy_level - 1 &&
           "Invalid phase in __sei_commit()");
    //fprintf(stderr, "[VERIFICATION] Final phase %d completed, proceeding to commit\n", current_phase);
    DLOG2("All %d phases completed, performing N-way verification\n",
          redundancy_level);

#ifdef SEI_CPU_ISOLATION
    /* Automatic retry loop for SDC recovery */
    while (1) {
        /* Attempt non-destructive commit (DMR verification) */
        if (sei_try_commit(__sei_thread->sei)) {
            /* Verification succeeded - proceed with actual commit */
            sei_commit(__sei_thread->sei);
            break;  /* Success - exit retry loop */
        }

        /* SDC detected - automatic recovery */
        int current_core = sched_getcpu();
        //fprintf(stderr, "[libsei] SDC detected on core %d, recovering...\n", current_core);

        /* Step 0: Set sei->p to -1 to indicate we are outside transaction
         * This prevents pthread wrappers in cpu_isolation functions from
         * trying to record operations to abuf, which would cause state
         * inconsistencies during recovery. */
        sei_setp(__sei_thread->sei, -1);

        /* Step 1: Blacklist the faulty core(s) */
#ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
        /* In phase migration mode: blacklist both phase0 and phase1 cores */
        cpu_isolation_blacklist_core(phase0_core);  /* phase0 core */
        cpu_isolation_blacklist_core(current_core);  /* phase1 core */
#else
        /* Traditional mode: blacklist only the current core */
        cpu_isolation_blacklist_current();
#endif

        /* Step 2: Migrate to another core (exits if all cores blacklisted) */
        cpu_isolation_migrate_current_thread();

        /* Step 3: Rollback transaction state (sets sei->p back to 0) */
        sei_rollback(__sei_thread->sei);

        /* Step 3.5: Clean up thread-local buffers not managed by sei_t */
#ifdef SEI_WRAP_SC
        abuf_clean(__sei_thread->abuf_sc);
#endif
#ifdef SEI_MT
        abuf_clean(__sei_thread->abuf);
#endif

#ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
        /* In phase migration mode: reset phase0_core for the retry
         * The retry will execute phase0 on the current (new) core,
         * and phase1 will migrate to yet another core */
        phase0_core = -1;
#endif

        /* Step 4: Retry from p=0 using context switch
         * Note: sei_rollback() already set sei->p = 0, and we need to
         * re-execute the entire transaction from the beginning (p=0).
         * Therefore, we pass 0x00 to __sei_switch() to make _ITM_beginTransaction
         * return 0 (p=0), triggering the first pass of DMR. */
        __sei_switch(&__sei_thread->ctx, 0x00);

        /* Loop continues - retry transaction on new core */
    }
#else
    /* Traditional commit without CPU isolation */
    sei_commit(__sei_thread->sei);
#endif /* SEI_CPU_ISOLATION */

#ifdef SEI_2PL
    int r = 0;
    pthread_mutex_t* l = NULL;
    assert (sei_getp(__sei_thread->sei) == -1);
    abuf_rewind(__sei_thread->abuf);
    while (abuf_size(__sei_thread->abuf)) {
        l = abuf_pop(__sei_thread->abuf, (void*) &r);
        // we only pushed locks and trylocks, hence if r == 0, l was
        // successfully locked.
        DLOG3("late unlocking %p (thread = %p)\n", l, (void*) pthread_self());
        if (!r) {
            r = __pthread_mutex_unlock(l);
            assert (!r && "unlock failed");
        }
    }
    //abuf_clean(__sei_thread->abuf_2pl);
#endif

#ifdef SEI_MT
    abuf_clean(__sei_thread->abuf);
#endif

#ifdef SEI_TBAR
    tbar_leave(__sei_thread->tbar);
#endif /* SEI_TBAR */

#ifdef SEI_WRAP_SC
    abuf_clean(__sei_thread->abuf_sc);
#endif /* SEI_WRAP_SC */
}
#endif /* ! SEI_MTL */

int
__sei_prepare(const void* ptr, size_t size, uint32_t crc, int ro)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif
    /* Reset redundancy level to default before preparing transaction */
    sei_set_redundancy(__sei_thread->sei, SEI_DMR_REDUNDANCY);

    return sei_prepare(__sei_thread->sei, ptr, size, crc, ro);
}

void
__sei_prepare_nm(const void* ptr, size_t size, uint32_t crc, int ro)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif
    /* Reset redundancy level to default before preparing transaction */
    sei_set_redundancy(__sei_thread->sei, SEI_DMR_REDUNDANCY);

    memset(__sei_ignore_addr_s, 0, sizeof(__sei_ignore_addr_s));
    memset(__sei_ignore_addr_e, 0, sizeof(__sei_ignore_addr_e));
    __sei_ignore_num = 0;
    sei_prepare_nm(__sei_thread->sei);
}

int
__sei_prepare_n(const void* ptr, size_t size, uint32_t crc, int ro, int redundancy_level)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif
    /* Set redundancy level before preparing transaction */
    sei_set_redundancy(__sei_thread->sei, redundancy_level);
    return sei_prepare(__sei_thread->sei, ptr, size, crc, ro);
}

void
__sei_prepare_nm_n(int redundancy_level)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif
    /* Set redundancy level before preparing transaction */
    sei_set_redundancy(__sei_thread->sei, redundancy_level);
    memset(__sei_ignore_addr_s, 0, sizeof(__sei_ignore_addr_s));
    memset(__sei_ignore_addr_e, 0, sizeof(__sei_ignore_addr_e));
    __sei_ignore_num = 0;
    sei_prepare_nm(__sei_thread->sei);
}

void
__sei_output_append(const void* ptr, size_t size)
{
    sei_output_append(__sei_thread->sei, ptr, size);
}

void
__sei_output_done()
{
    sei_output_done(__sei_thread->sei);
}

uint32_t
__sei_output_next()
{
    return sei_output_next(__sei_thread->sei);
}

void
__sei_unprotect(void* addr, size_t size)
{
#ifdef HEAP_PROTECT
    sei_unprotect(__sei_thread->sei, addr, size);
#endif
    // else ignore
}

/* ----------------------------------------------------------------------------
 * Traversal management
 * ------------------------------------------------------------------------- */

int
__sei_bar()
{
#ifdef SEI_TBAR
    return !tbar_check(__sei_thread->tbar);
#else /* SEI_TBAR */
    return 0;
#endif /* SEI_TBAR */
}

int
__sei_shift(int handle)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif

#ifdef SEI_TBAR
    // assume we have correct handle already in-place
    if (handle == -1) {
        // create new obuf and exchange; use current if first time */
        if (stash_size(__sei_thread->stash) != 0) {
            // here we assume that current tbar already in stash
            __sei_thread->tbar = tbar_idup(__sei_thread->tbar);
        }
        // add to stash
#ifndef NDEBUG
        //TODO: int h =
#endif
            sei_shift(__sei_thread->sei, handle);
        handle = stash_add(__sei_thread->stash, __sei_thread->tbar);
        //TODO: assert (handle == h);
    } else {
        // shift and exchange tbar
#ifndef NDEBUG
        //TODO: int h =
#endif
            sei_shift(__sei_thread->sei, handle);
        //TODO: assert (handle == h);
        __sei_thread->tbar = stash_get(__sei_thread->stash, handle);
        assert (__sei_thread->tbar);
    }
    return handle;
#else /* SEI_TBAR */
    return sei_shift(__sei_thread->sei, handle);
#endif /* SEI_TBAR */
}
