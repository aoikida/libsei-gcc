/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <asco.h>
#include "debug.h"

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

#include "heap.h"

#include "tbin.h"
#include "talloc.h"
#include "obuf.h"
#include "ibuf.h"
#include "cfc.h"

#ifdef COW_APPEND_ONLY
# ifndef COWBACK
#  error COW_APPEND_ONLY can only work with COWBACK
# endif
# include "abuf.h"
#else
# include "cow.h"
#endif

#ifdef HEAP_PROTECT
# include "abuf.h"
# include "protect.h"
#endif

#ifdef COW_USEHEAP
# define HEAP_SIZE (HEAP_1GB + HEAP_500MB)
#endif

#ifdef ASCO_STATS
#include "ilog.h"
#include "cpu_stats.h"
#include "now.h"
#endif

struct asco {
    int       p;       /* the actual process (0 or 1) */
    heap_t*   heap;    /* optional heap               */
#ifndef COW_APPEND_ONLY
    cow_t*    cow[2];  /* a copy-on-write buffer      */
#else
    abuf_t*   cow[2];  /* a copy-on-write buffer      */
#endif
    tbin_t*   tbin;    /* trash bin for delayed frees */
    talloc_t* talloc;  /* traversal allocator         */
    obuf_t*   obuf;    /* output buffer (messages)    */
    ibuf_t*   ibuf;    /* input message buffer        */
    cfc_t     cf[2];   /* control flags               */

#ifdef HEAP_PROTECT
    abuf_t*   wpages;  /* list of written pages       */
#endif

#ifdef ASCO_STATS
    ilog_t*   ilog;    /* stats logger                */

    struct {
        unsigned int ntrav;      /* traversal count   */
        unsigned int nmalloc;    /* malloc count      */
        unsigned int nfree;      /* malloc count      */
        unsigned int nwuint8_t;  /* write 8 count     */
        unsigned int nwuint16_t; /* write 16 count    */
        unsigned int nwuint32_t; /* write 32 count    */
        unsigned int nwuint64_t; /* write 64 count    */
        unsigned int nprotect;   /* number of pages   */
    } stats;           /* general statistics          */

    cpu_stats_t* cpu_stats; /* cpu usage statistics   */
#endif
};

/* -----------------------------------------------------------------------------
 * stats helpers
 * -------------------------------------------------------------------------- */

#ifdef ASCO_STATS
#define ASCO_STATS_RESET() do {                   \
        asco->stats.ntrav      = 0;               \
        asco->stats.nmalloc    = 0;               \
        asco->stats.nfree      = 0;               \
        asco->stats.nwuint8_t  = 0;               \
        asco->stats.nwuint16_t = 0;               \
        asco->stats.nwuint32_t = 0;               \
        asco->stats.nwuint64_t = 0;               \
        asco->stats.nprotect   = 0;               \
    } while (0)
#define ASCO_STATS_INIT() do {                          \
        asco->ilog = ilog_init("asco-stats.log");       \
        ASCO_STATS_RESET();                             \
        asco->cpu_stats = cpu_stats_init();             \
    } while (0)
#define ASCO_STATS_FINI() do {           \
        ilog_fini(asco->ilog);           \
        cpu_stats_fini(asco->cpu_stats); \
    } while (0)
#define ASCO_STATS_INC(X) (++asco->stats.X)
#define ASCO_STATS_REPORT() do {                                        \
        static uint64_t _now = 0;                                       \
        if (now() - _now > NOW_1S) {                                    \
            char buffer[1024];                                          \
            sprintf(buffer, "%u %u %u %u %u %u %u %u",                  \
                    asco->stats.ntrav,                                  \
                    asco->stats.nmalloc,                                \
                    asco->stats.nfree,                                  \
                    asco->stats.nwuint8_t,                              \
                    asco->stats.nwuint16_t,                             \
                    asco->stats.nwuint32_t,                             \
                    asco->stats.nwuint64_t,                             \
                    asco->stats.nprotect                                \
                );                                                      \
            ilog_push(asco->ilog, __FILE__, buffer);                    \
            cpu_stats_report(asco->cpu_stats, asco->ilog);              \
            _now = now();                                               \
            ASCO_STATS_RESET();                                         \
        }                                                               \
    } while (0)
#else
#define ASCO_STATS_INIT()
#define ASCO_STATS_FINI()
#define ASCO_STATS_RESET()
#define ASCO_STATS_INC(X)
#define ASCO_STATS_REPORT()
#endif

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

asco_t*
asco_init()
{
    asco_t* asco = (asco_t*) malloc(sizeof(asco_t));
    assert(asco);
#ifndef COW_APPEND_ONLY
    asco->cow[0] = cow_init(0, 100000);
    asco->cow[1] = cow_init(0, 100000);
#else
    asco->cow[0] = abuf_init(100000);
    asco->cow[1] = abuf_init(100000);
#endif

#ifdef HEAP_PROTECT
    asco->wpages = abuf_init(100);
#endif

#ifdef COW_USEHEAP
    asco->heap   = heap_init(HEAP_SIZE);

#if defined(HEAP_PROTECT) && HEAP_SIZE != HEAP_NP
    // if the heap is preallocated, protect whole heap
    //protect_mem(asco->heap, HEAP_SIZE + sizeof(heap_t), READ);
#endif

#else  /* !COW_USEHEAP */
    asco->heap   = NULL;
#endif /* !COW_USEHEAP */

    asco->tbin   = tbin_init(100, asco->heap);
    asco->talloc = talloc_init(asco->heap);
    asco->obuf   = obuf_init(10); // 10 messages most
    asco->ibuf   = ibuf_init();

    ASCO_STATS_INIT();

    // initialize with invalid execution number
    asco->p = -1;

    DLOG3("asco_init addr: %p (heap = {%p})\n", asco, asco->heap);

    return asco;
}

void
asco_fini(asco_t* asco)
{
    assert(asco);
#ifndef COW_APPEND_ONLY
    cow_fini(asco->cow[0]);
    cow_fini(asco->cow[1]);
#else
    abuf_fini(asco->cow[0]);
    abuf_fini(asco->cow[1]);
#endif

    tbin_fini(asco->tbin);
    talloc_fini(asco->talloc);
    obuf_fini(asco->obuf);
    ibuf_fini(asco->ibuf);

#ifdef COW_USEHEAP
    heap_fini(asco->heap);
#endif

#ifdef HEAP_PROTECT
    abuf_fini(asco->wpages);
#endif

    ASCO_STATS_FINI();
}


/* -----------------------------------------------------------------------------
 * traversal control
 * -------------------------------------------------------------------------- */

int
asco_prepare(asco_t* asco, const void* ptr, size_t size, uint32_t crc, int ro)
{
    assert (ptr != NULL);
    assert (asco->p == -1);

    // check input message
    return ibuf_prepare(asco->ibuf, ptr, size, crc, ro ? READ_ONLY:READ_WRITE);
}

void
asco_prepare_nm(asco_t* asco)
{
    // empty message
    (void) ibuf_prepare(asco->ibuf, NULL, 0, crc_init(), READ_ONLY);
}

void
asco_begin(asco_t* asco)
{
    if (asco->p == -1) {
        DLOG2("First execution\n");
        asco->p = 0;
        //assert (obuf_size(asco->obuf) == 0);
        cfc_reset(&asco->cf[0]);
        cfc_reset(&asco->cf[1]);
    }

    if (asco->p == 1) {
        DLOG2("Second execution\n");
    }
}

void
asco_switch(asco_t* asco)
{
    DLOG2("Switch: %d\n", asco->p);
    asco->p = 1;
    DLOG2("Switched: %d\n", asco->p);
    talloc_switch(asco->talloc);
    obuf_close(asco->obuf);
    ibuf_switch(asco->ibuf);

#ifdef COWBACK
#ifdef COW_APPEND_ONLY
    abuf_swap(asco->cow[0]);
#else
    cow_swap(asco->cow[0]);
#endif
#endif
    cfc_alog(&asco->cf[0]);
    int r = cfc_amog(&asco->cf[0]);
    assert (r && "control flow error");
}

void
asco_commit(asco_t* asco)
{
    DLOG2("COMMIT: %d\n", asco->p);
    asco->p = -1;

    int r = cfc_amog(&asco->cf[1]);
    assert (r && "control flow error");
    cfc_alog(&asco->cf[1]);

#ifndef COW_APPEND_ONLY
    cow_show(asco->cow[0]);
    cow_show(asco->cow[1]);
    cow_apply_cmp(asco->cow[0], asco->cow[1]);
#else
    abuf_cmp_heap(asco->cow[0], asco->cow[1]);
    abuf_clean(asco->cow[0]);
    abuf_clean(asco->cow[1]);
#endif

    tbin_flush(asco->tbin);
    talloc_clean(asco->talloc);
    obuf_close(asco->obuf);

    r = cfc_check(&asco->cf[0]);
    assert (r && "control flow error");
    r = cfc_check(&asco->cf[1]);
    assert (r && "control flow error");

    r = ibuf_correct(asco->ibuf);
    assert (r == 1 && "input message modified");


    ASCO_STATS_INC(ntrav);
    ASCO_STATS_REPORT();
#ifdef HEAP_PROTECT
    int i;
    for (i = 0; i < abuf_size(asco->wpages); ++i) {
        uint64_t size;
        void* ptr = abuf_pop(asco->wpages, &size);
        protect_mem(ptr, size, READ);
        ASCO_STATS_INC(nprotect);
    }
    abuf_clean(asco->wpages);
#endif
}

inline int
asco_getp(asco_t* asco)
{
    return asco->p;
}

inline void
asco_setp(asco_t* asco, int p)
{
    asco->p = p;
}

/* -----------------------------------------------------------------------------
 * memory management
 * -------------------------------------------------------------------------- */

inline void*
asco_malloc(asco_t* asco, size_t size)
{
    ASCO_STATS_INC(nmalloc);
    void* ptr = talloc_malloc(asco->talloc, size);
#if defined(HEAP_PROTECT)
    // && (!defined(COW_USEHEAP) || HEAP_SIZE == HEAP_NP)
    // if heap has to be protected and
    // either we don't use heap_t or
    // we do use heap_t but it's not preallocated (HEAP_NP)
    // then we have to protect the heap whever we do a malloc

    if (asco->p == 0) {
        // we don't have to protect that for the second execution
        protect_mem(ptr, size, READ);
    }
#endif
    return ptr;
}

inline void
asco_free(asco_t* asco, void* ptr)
{
    ASCO_STATS_INC(nfree);
    tbin_add(asco->tbin, ptr, asco->p);
}

void*
asco_calloc(asco_t* asco, size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
}

/* -----------------------------------------------------------------------------
 * memory management outside traversal
 * -------------------------------------------------------------------------- */

void*
asco_malloc2(asco_t* asco, size_t size)
{
    return malloc(size);
}

void
asco_free2(asco_t* asco, void* ptr1, void* ptr2)
{
    assert (0 && "asco not compiled with HEAP_MODE");
}

inline void*
asco_other(asco_t* asco, void* addr)
{
    assert (0 && "asco not compiled with HEAP_MODE");
    return NULL;
}

void*
asco_memcpy2(asco_t* asco, void* dest, const void* src, size_t n)
{
    assert (0 && "asco not compiled with HEAP_MODE");
    return NULL;
}

#ifdef HEAP_PROTECT
void
asco_unprotect(asco_t* asco, void* addr, size_t size)
{
    if (asco->p == 1) {
        assert (0 && "straaaange");
    }
    if (asco->p == 0 || asco->p == -1) {
        abuf_push(asco->wpages, addr, size);
        protect_mem(addr, size, WRITE);
    }
}
#endif


/* -----------------------------------------------------------------------------
 * load and stores
 * -------------------------------------------------------------------------- */

#ifndef COW_APPEND_ONLY
#define ASCO_READ(type) inline                                          \
    type asco_read_##type(asco_t* asco, const type* addr)               \
    {                                                                   \
        DLOG3("asco_read_%s(%d) addr = %p", #type, asco->p, addr);      \
        cow_t* cow = asco->cow[asco->p];                                \
        type value = cow_read_##type(cow, addr);                        \
        DLOG3("= %lx, %lx\n", (uint64_t) *addr, value);                 \
        return value;                                                   \
    }
ASCO_READ(uint8_t)
ASCO_READ(uint16_t)
ASCO_READ(uint32_t)
ASCO_READ(uint64_t)

#define ASCO_WRITE(type) inline                                         \
    void asco_write_##type(asco_t* asco, type* addr, type value)        \
    {                                                                   \
        ASCO_STATS_INC(nw##type);                                       \
        assert (asco->p == 0 || asco->p == 1);                          \
        DLOG3("asco_write_%s(%d): %p <- %llx\n", #type, asco->p,        \
              addr, (uint64_t) value);                                  \
        cow_t* cow = asco->cow[asco->p];                                \
        cow_write_##type(cow, addr, value);                             \
    }
ASCO_WRITE(uint8_t)
ASCO_WRITE(uint16_t)
ASCO_WRITE(uint32_t)
ASCO_WRITE(uint64_t)
#else

#define ASCO_READ(type) inline                                          \
    type asco_read_##type(asco_t* asco, const type* addr)               \
    {                                                                   \
        DLOG3("asco_read_%s(%d) %p = %lx", #type, asco->p, addr,        \
              (uint64_t) *addr);                                        \
        return *addr;                                                   \
    }
ASCO_READ(uint8_t)
ASCO_READ(uint16_t)
ASCO_READ(uint32_t)
ASCO_READ(uint64_t)

#define ASCO_WRITE(type) inline                                         \
    void asco_write_##type(asco_t* asco, type* addr, type value)        \
    {                                                                   \
        assert (asco->p == 0 || asco->p == 1);                          \
        DLOG3("asco_write_%s(%d): %p <- %llx\n", #type, asco->p,        \
              addr, (uint64_t) value);                                  \
        abuf_push_##type(asco->cow[asco->p], addr, *addr);              \
        *addr = value;                                                  \
    }
ASCO_WRITE(uint8_t)
ASCO_WRITE(uint16_t)
ASCO_WRITE(uint32_t)
ASCO_WRITE(uint64_t)
#endif

/* -----------------------------------------------------------------------------
 * output messages
 * -------------------------------------------------------------------------- */

/* asco_output_append and asco_output_done can be called from outside
 * a handler with no effect.
 *
 * asco_output_next can only be called from outside the handler.
 */
void
asco_output_append(asco_t* asco, const void* ptr, size_t size)
{
    if (asco->p == -1) return;
    obuf_push(asco->obuf, ptr, size);
}

void
asco_output_done(asco_t* asco)
{
    if (asco->p == -1) return;
    obuf_done(asco->obuf);
}

uint32_t
asco_output_next(asco_t* asco)
{
    assert (asco->p == -1);
    assert (obuf_size(asco->obuf) > 0 && "no CRC to pop");
    uint32_t crc = obuf_pop(asco->obuf);

    return crc;
}
