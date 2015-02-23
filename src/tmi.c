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
#include "asco.h"
#include "debug.h"
#include "heap.h"
#include "cow.h"
#include "tmi_mt.h"
#include "config.h"

#ifdef SEI_WRAP_SC
#include "tmi_sc.h"
#include "wts.h"
#endif

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

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
    asco_t* asco;
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

/* initialize library and allocate an asco object. This is called once
 * the library is loaded. If static liked should be called on
 * initialization of the program. */
static void __attribute__((constructor))
__sei_init()
{
#ifndef SEI_MT
    assert (__sei_thread->asco == NULL);
    __sei_thread->asco = asco_init();
    assert (__sei_thread->asco);
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

    assert (__sei_thread->asco == NULL);
    __sei_thread->asco = asco_init();
    assert (__sei_thread->asco);
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

    assert (__sei_thread->asco);
    asco_fini(__sei_thread->asco);

#else /* SEI_MT */
    int i;
    for (i = 0; i < __sei_thread_count; ++i) {
        assert (___sei_thread[i].asco);
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
        asco_fini(___sei_thread[i].asco);
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
void* __asco_ignore_addr_s[SEI_MAX_IGNORE];
void* __asco_ignore_addr_e[SEI_MAX_IGNORE];
uint32_t __asco_ignore_num = 0;
uint32_t __asco_ignore_all = 0;
int __asco_write_disable = 0;
#endif

void __sei_ignore(int v) {
	__asco_write_disable = v;
}

void __sei_ignore_all(uint32_t v) {
	__asco_ignore_all = v;
}

void __sei_ignore_addr(void* start, void* end) {
	if (asco_getp(__sei_thread->asco) == -1)
		return;
	int i;
	for (i = 0; i < __asco_ignore_num; ++i) {
		if (__asco_ignore_addr_s[i] == start &&
		    __asco_ignore_addr_e[i] == end)
			return;
	}

	__asco_ignore_addr_s[__asco_ignore_num] = start;
	__asco_ignore_addr_e[__asco_ignore_num] = end;
	__asco_ignore_num++;
	assert(SEI_MAX_IGNORE >= __asco_ignore_num && "not enough ignore slots");
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
    if (__asco_write_disable || IN_STACK(ptr)) {
        DLOG3("Ignore address: %p\n", ptr);
        return 1;
    }// else return 0;

#if 1
//    if ((uintptr_t) ptr < (uintptr_t) &edata) return 1;

        int i;
        for (i = 0; i < __asco_ignore_num; ++i)
            if (ptr >= __asco_ignore_addr_s[i] && ptr <= __asco_ignore_addr_e[i]) {
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

inline void*
_ITM_malloc(size_t size)
{
    if (__asco_ignore_all) {
        void* r = malloc(size); //asco_malloc(__sei_thread->asco, size);
        __sei_ignore_addr(r, (uint8_t*)r + size);
        return r;
    } else return asco_malloc(__sei_thread->asco, size);
}

inline void
_ITM_free(void* ptr)
{
    int i;
    for (i = 0; i < __asco_ignore_num; ++i)
            if (ptr == __asco_ignore_addr_s[i]) {
                free(ptr);
                return;
            }
    asco_free(__sei_thread->asco, ptr);
}

inline void*
_ITM_calloc(size_t nmemb, size_t size)
{
    return asco_malloc(__sei_thread->asco, nmemb*size);
}
#ifndef COW_WT
#define ITM_READ(type, prefix, suffix) inline                   \
    type _ITM_R##prefix##suffix(const type* addr)               \
    {                                                           \
        if (ignore_addr(addr)) return *addr;                    \
        else return asco_read_##type(__sei_thread->asco, addr);     \
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
            asco_write_##type(__sei_thread->asco, addr, value);     \
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
				asco_write_uint8_t(__sei_thread->asco, (void*) (destination + i),
						   *(uint8_t*) (source + i));
			} while (++i < unal);
		}

    	len -= i;

    	uint32_t unal64 = (unsigned long int)(destination + i) % sizeof(uint64_t);

    	if (unal64 > 0 && len >= sizeof(uint32_t)) {
    		asco_write_uint32_t(__sei_thread->asco, (void*) (destination + i),
    							   *(uint32_t*) (source + i));

    		i += sizeof(uint32_t);
    		len -= sizeof(uint32_t);
    	}

    	uint32_t num64w = len / sizeof(uint64_t);
		uint32_t j = 0;

		while (j < num64w) {
			asco_write_uint64_t(__sei_thread->asco, (void*) (destination + i),
					   *(uint64_t*) (source + i));
			i += sizeof(uint64_t);
			len -= sizeof(uint64_t);
			j++;
		};

		if (len >= sizeof(uint32_t)) {
			asco_write_uint32_t(__sei_thread->asco, (void*) (destination + i),
							   *(uint32_t*) (source + i));

			i += sizeof(uint32_t);
		}

		while (i < size) {
			asco_write_uint8_t(__sei_thread->asco, (void*) (destination + i),
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
        asco_write_uint8_t(__sei_thread->asco, (void*) (destination + i),
                           *(uint8_t*) (source + i));
#else
        asco_write_uint8_t(__sei_thread->asco, (void*) (destination + i),
                           asco_read_uint8_t(__sei_thread->asco,
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
        asco_write_uint8_t(__sei_thread->asco, (void*) (p++), c);

    while (p < e64) {
        asco_write_uint64_t(__sei_thread->asco, (void*) (p), v);
        p += 8;
    }

    while (p < e)
        asco_write_uint8_t(__sei_thread->asco, (void*) (p++), c);

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

    switch (asco_getp(__sei_thread->asco)) {
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
    int p = asco_getp(__sei_thread->asco);

    switch (p) {
    case 0:
    case 1: {
    	DLOG3("got args for close: %d\n", fd);
    	wts_add(asco_get_wts(__sei_thread->asco), p, wts_close, 2, (uint64_t) fd,
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

    switch (asco_getp(__sei_thread->asco)) {
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

    switch (asco_getp(__sei_thread->asco)) {
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
    int p = asco_getp(__sei_thread->asco);

    switch (p) {
    case 0:
    case 1: {
    	wts_add(asco_get_wts(__sei_thread->asco), p, wts_send, 5, (uint64_t) socket,
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
	int p = asco_getp(__sei_thread->asco);

	switch (p) {
	case 0:
	case 1: {

		if (IN_STACK(dest_addr)) {
			assert(sizeof(struct sockaddr) == 16);

			uint64_t a1, a2;
			unsigned char* d = (unsigned char*) dest_addr;
			memcpy(&a1, d, 8);
			memcpy(&a2, d + 8, 8);

			wts_add(asco_get_wts(__sei_thread->asco), p, wts_sendto, 10,
								(uint64_t) socket, (uint64_t) buffer,
								(uint64_t) size, (uint64_t) flags,
								(uint64_t) dest_addr, (uint64_t) addrlen,
								(uint64_t) size, 1/*dest_addr on stack*/, a1, a2);
		}
		else
			wts_add(asco_get_wts(__sei_thread->asco), p, wts_sendto, 8,
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
    asco_prepare_nm(__sei_thread->asco);

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

    switch (asco_getp(__sei_thread->asco)) {
#ifdef SEI_MTL2
    case 0:
    case 1:
        __sei_commit(1);
        r =  __pthread_mutex_lock(lock);
        __sei_mtl(getbp());
        break;

#else /* SEI_MTL2 */
    case 0:
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
#ifdef SEI_2PL
        //abuf_push_uint64_t(__sei_thread->abuf_2pl, (uint64_t*) lock, r);
#endif /* SEI_2PL */
        break;
    case 1:
        DLOG3("fake locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
        break;
#endif /* SEI_MTL2 */
    default:
        DLOG3("locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_lock(lock);
        break;
    }

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
    switch (asco_getp(__sei_thread->asco)) {

#ifdef SEI_MTL2
    case 0:
    case 1:
        __sei_commit(1);
        r =  __pthread_mutex_trylock(lock);
        __sei_mtl(getbp());
        break;
#else
    case 0:
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
#ifdef SEI_2PL
        //abuf_push_uint64_t(__sei_thread->abuf_2pl, (uint64_t*) lock, r);
#endif /* SEI_2PL */
        break;
    case 1:
        DLOG3("fake trylock %p (thread = %p)\n", lock, (void*) pthread_self());
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
        break;
#endif /* SEI_MTL2 */
    default:
        DLOG3("try locking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_trylock(lock);
        break;
    }

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
    switch (asco_getp(__sei_thread->asco)) {
    case 0:
    case 1:
        __sei_commit(1);
        DLOG3( "unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r =  __pthread_mutex_unlock(lock);
        DLOG3( "start mini traversal (thread = %p)\n", (void*) pthread_self());
        __sei_mtl(getbp());
        break;
    default:
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
    switch (asco_getp(__sei_thread->asco)) {
#ifndef SEI_2PL
    case 0:
        r = __pthread_mutex_unlock(lock);
        abuf_push_uint64_t(__sei_thread->abuf, (uint64_t*) lock, r);
        break;
    case 1:
        r = abuf_pop_uint64_t(__sei_thread->abuf, (uint64_t*) lock);
        break;
#else /* SEI_2PL */
    case 0:
    case 1:
        r = 0; // 0 for successful unlock
        break;
#endif /* SEI_2PL */
    default:
        DLOG3("unlocking %p (thread = %p)\n", lock, (void*) pthread_self());
        r = __pthread_mutex_unlock(lock);
        break;
    }

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
    assert (asco_getp(__sei_thread->asco) == -1
            && "called from transactional code");
    return asco_malloc2(__sei_thread->asco, size);
}

void*
tanger_txnal_sei_thread_malloc(size_t size)
{
    assert (asco_getp(__sei_thread->asco) != -1
            && "called from non-transactional code");
    return asco_malloc(__sei_thread->asco, size);
}

void*
__sei_other(void* ptr)
{
    int p = asco_getp(__sei_thread->asco);
    asco_setp(__sei_thread->asco, 0);
    void* r = asco_other(__sei_thread->asco, ptr);
    asco_setp(__sei_thread->asco, p);
    return r;
}

uint32_t
__sei_begin(sei_thread_ctx_t* ctx)
{
#ifdef SEI_MT
    assert (__sei_thread && "sei_thread_prepare should be called before begin");
#ifdef SEI_TBAR
    tbar_enter(__sei_thread->tbar);
#endif /* SEI_TBAR */
#endif /* SEI_MT */
    memcpy(&__sei_thread->ctx, ctx, sizeof(sei_thread_ctx_t));
    __sei_thread->high = __sei_thread->ctx.rbp;
    asco_begin(__sei_thread->asco);
    return 0x01;
}

#ifdef SEI_MTL
void
__sei_commit(int force)
{
    if (!asco_getp(__sei_thread->asco)) {
        asco_switch(__sei_thread->asco);
        //fprintf(stderr, "Acquired locks: %d\n", abuf_size(__sei_thread->abuf));

        if (__sei_thread->mtl) {
            // copy stack back
            __sei_switch2((void*)__sei_thread->rsp, __sei_thread->stack,
                           __sei_thread->size, &__sei_thread->ctx, 0x01);
        } else {
            __sei_switch(&__sei_thread->ctx, 0x01);
        }
    }
    asco_commit(__sei_thread->asco);

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
#if 1
    //	memset(__asco_ignore_addr_s, 0, sizeof(__asco_ignore_addr_s));
    // 	memset(__asco_ignore_addr_e, 0, sizeof(__asco_ignore_addr_e));
    DLOG1("__asco_ignore_num = %d\n", __asco_ignore_num);
	__asco_ignore_num = 0;
    __asco_write_disable = 0;
#endif

    if (!asco_getp(__sei_thread->asco)) {
        asco_switch(__sei_thread->asco);
        __sei_switch(&__sei_thread->ctx, 0x01);
    }
    asco_commit(__sei_thread->asco);

#ifdef SEI_2PL
    int r = 0;
    pthread_mutex_t* l = NULL;
    assert (asco_getp(__sei_thread->asco) == -1);
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
    return asco_prepare(__sei_thread->asco, ptr, size, crc, ro);
}

void
__sei_prepare_nm(const void* ptr, size_t size, uint32_t crc, int ro)
{
#ifdef SEI_MT
    if (unlikely(!__sei_thread)) __sei_thread_init();
#endif
    memset(__asco_ignore_addr_s, 0, sizeof(__asco_ignore_addr_s));
    memset(__asco_ignore_addr_e, 0, sizeof(__asco_ignore_addr_e));
    __asco_ignore_num = 0;
    asco_prepare_nm(__sei_thread->asco);
}

void
__sei_output_append(const void* ptr, size_t size)
{
    asco_output_append(__sei_thread->asco, ptr, size);
}

void
__sei_output_done()
{
    asco_output_done(__sei_thread->asco);
}

uint32_t
__sei_output_next()
{
    return asco_output_next(__sei_thread->asco);
}

void
__sei_unprotect(void* addr, size_t size)
{
#ifdef HEAP_PROTECT
    asco_unprotect(__sei_thread->asco, addr, size);
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
            asco_shift(__sei_thread->asco, handle);
        handle = stash_add(__sei_thread->stash, __sei_thread->tbar);
        //TODO: assert (handle == h);
    } else {
        // shift and exchange tbar
#ifndef NDEBUG
        //TODO: int h =
#endif
            asco_shift(__sei_thread->asco, handle);
        //TODO: assert (handle == h);
        __sei_thread->tbar = stash_get(__sei_thread->stash, handle);
        assert (__sei_thread->tbar);
    }
    return handle;
#else /* SEI_TBAR */
    return asco_shift(__sei_thread->asco, handle);
#endif /* SEI_TBAR */
}
