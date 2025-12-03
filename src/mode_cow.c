/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014,2015 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include "sei.h"
#include "debug.h"
#include "fail.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */
#include "tbin.h"
#include "talloc.h"
#include "obuf.h"
#include "ibuf.h"
#include "cfc.h"
#include "stash.h"
#include "config.h"

#ifdef SEI_WRAP_SC
#include "wts.h"
#endif

#ifdef COW_APPEND_ONLY
# ifndef COW_WT
#  error COW_APPEND_ONLY can only work with COW_WT
# endif
# include "abuf.h"
#else
# include "cow.h"
#endif

#ifdef COW_USEHEAP
# include "heap.h"
#endif

#ifdef HEAP_PROTECT
# include "abuf.h"
# include "protect.h"
#endif

#ifdef COW_USEHEAP
# define HEAP_SIZE (HEAP_1GB + HEAP_500MB)
#endif

#ifdef SEI_STATS
# include "ilog.h"
# include "cpu_stats.h"
# include "now.h"
#endif

struct sei {
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
    stash_t*  stash;   /* obuf contexts               */
#ifdef SEI_WRAP_SC
    wts_t*	  wts;	   /* waitress for delayed calls  */
#endif

#ifdef HEAP_PROTECT
    abuf_t*   wpages;  /* list of written pages       */
#endif

#ifdef SEI_STATS
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

/* ----------------------------------------------------------------------------
 * stats helpers
 * ------------------------------------------------------------------------- */

#ifdef SEI_STATS
#define SEI_STATS_RESET() do {                   \
        sei->stats.ntrav      = 0;               \
        sei->stats.nmalloc    = 0;               \
        sei->stats.nfree      = 0;               \
        sei->stats.nwuint8_t  = 0;               \
        sei->stats.nwuint16_t = 0;               \
        sei->stats.nwuint32_t = 0;               \
        sei->stats.nwuint64_t = 0;               \
        sei->stats.nprotect   = 0;               \
    } while (0)
#define SEI_STATS_INIT() do {                           \
        sei->ilog = ilog_init("sei-stats.log");         \
        SEI_STATS_RESET();                              \
        sei->cpu_stats = cpu_stats_init();              \
    } while (0)
#define SEI_STATS_FINI() do {           \
        ilog_fini(sei->ilog);           \
        cpu_stats_fini(sei->cpu_stats); \
    } while (0)
#define SEI_STATS_INC(X) (++sei->stats.X)
#define SEI_STATS_REPORT() do {                                         \
        static uint64_t _now = 0;                                       \
        if (now() - _now > NOW_1S) {                                    \
            char buffer[1024];                                          \
            sprintf(buffer, "%u %u %u %u %u %u %u %u",                  \
                    sei->stats.ntrav,                                   \
                    sei->stats.nmalloc,                                 \
                    sei->stats.nfree,                                   \
                    sei->stats.nwuint8_t,                               \
                    sei->stats.nwuint16_t,                              \
                    sei->stats.nwuint32_t,                              \
                    sei->stats.nwuint64_t,                              \
                    sei->stats.nprotect                                 \
                );                                                      \
            ilog_push(sei->ilog, __FILE__, buffer);                     \
            cpu_stats_report(sei->cpu_stats, sei->ilog);                \
            _now = now();                                               \
        }                                                               \
    } while (0)
#else
#define SEI_STATS_INIT()
#define SEI_STATS_FINI()
#define SEI_STATS_RESET()
#define SEI_STATS_INC(X)
#define SEI_STATS_REPORT()
#endif



/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

sei_t*
sei_init()
{
    sei_t* sei = (sei_t*) malloc(sizeof(sei_t));
    assert(sei);
#ifndef COW_APPEND_ONLY
    sei->cow[0] = cow_init(0, COW_SIZE);
    sei->cow[1] = cow_init(0, COW_SIZE);
#else
    sei->cow[0] = abuf_init(COW_SIZE);
    sei->cow[1] = abuf_init(COW_SIZE);
#endif

#ifdef HEAP_PROTECT
    sei->wpages = abuf_init(100);
#endif

#ifdef COW_USEHEAP
    sei->heap   = heap_init(HEAP_SIZE);

#if defined(HEAP_PROTECT) && HEAP_SIZE != HEAP_NP
    // if the heap is preallocated, protect whole heap
    //protect_mem(sei->heap, HEAP_SIZE + sizeof(heap_t), READ);
#endif

#else  /* !COW_USEHEAP */
    sei->heap   = NULL;
#endif /* !COW_USEHEAP */

    sei->tbin   = tbin_init(TBIN_SIZE, sei->heap);
    sei->talloc = talloc_init(sei->heap);
    sei->obuf   = obuf_init(OBUF_SIZE);
    sei->ibuf   = ibuf_init();
    sei->stash  = stash_init();
#ifdef SEI_WRAP_SC
    sei->wts	 = wts_init(SC_MAX_CALLS);
#endif
    SEI_STATS_INIT();

    // initialize with invalid execution number
    sei->p = -1;

    DLOG3("sei_init addr: %p (heap = {%p})\n", sei, sei->heap);

    return sei;
}

void
sei_fini(sei_t* sei)
{
    assert(sei);
#ifndef COW_APPEND_ONLY
    cow_fini(sei->cow[0]);
    cow_fini(sei->cow[1]);
#else
    abuf_fini(sei->cow[0]);
    abuf_fini(sei->cow[1]);
#endif

    tbin_fini(sei->tbin);
    talloc_fini(sei->talloc);
    ibuf_fini(sei->ibuf);
    if (stash_size(sei->stash)) {
        int i;
        for (i = 0; i < stash_size(sei->stash); ++i) {
            obuf_fini((obuf_t*) stash_get(sei->stash, i));
        }
    } else {
        obuf_fini(sei->obuf);
    }

#ifdef SEI_WRAP_SC
    wts_fini(sei->wts);
#endif

#ifdef COW_USEHEAP
    heap_fini(sei->heap);
#endif

#ifdef HEAP_PROTECT
    abuf_fini(sei->wpages);
#endif

    SEI_STATS_FINI();
}


/* ----------------------------------------------------------------------------
 * traversal control
 * ------------------------------------------------------------------------- */

int
sei_prepare(sei_t* sei, const void* ptr, size_t size, uint32_t crc, int ro)
{
    assert (ptr != NULL);
    assert (sei->p == -1);

    // check input message
    return ibuf_prepare(sei->ibuf, ptr, size, crc, ro ? READ_ONLY:READ_WRITE);
}

void
sei_prepare_nm(sei_t* sei)
{
    // empty message
    (void) ibuf_prepare(sei->ibuf, NULL, 0, crc_init(), READ_ONLY);
}

void
sei_begin(sei_t* sei)
{
    if (sei->p == -1) {
        DLOG2("First execution\n");
        sei->p = 0;
        //assert (obuf_size(sei->obuf) == 0);
        cfc_reset(&sei->cf[0]);
        cfc_reset(&sei->cf[1]);
    }

    if (sei->p == 1) {
        DLOG2("Second execution\n");
    }
}

void
sei_switch(sei_t* sei)
{
    DLOG2("Switch: %d\n", sei->p);
    sei->p = 1;
    DLOG2("Switched: %d\n", sei->p);
    talloc_switch(sei->talloc);
    obuf_close(sei->obuf);
    ibuf_switch(sei->ibuf);

#ifdef COW_WT
#ifdef COW_APPEND_ONLY
    /* In COW_WT mode, abuf_swap() restores OLD values to memory after p=0
     * execution, allowing p=1 to record the same OLD values for correct
     * DMR verification. Without this, p=1 would read NEW values from memory
     * (written by p=0), causing cow[0] and cow[1] to mismatch. */
    abuf_swap(sei->cow[0]);
#else
    cow_swap(sei->cow[0]);
#endif
#endif
    cfc_alog(&sei->cf[0]);
    int r = cfc_amog(&sei->cf[0]);
    fail_ifn(r, "control flow error");
}

void
sei_commit(sei_t* sei)
{
    DLOG2("COMMIT: %d\n", sei->p);
    sei->p = -1;

    int r = cfc_amog(&sei->cf[1]);
    fail_ifn(r, "control flow error");
    cfc_alog(&sei->cf[1]);

#ifndef COW_APPEND_ONLY
    cow_show(sei->cow[0]);
    cow_show(sei->cow[1]);
    cow_apply_cmp(sei->cow[0], sei->cow[1]);
#else
    abuf_cmp_heap(sei->cow[0], sei->cow[1]);
    abuf_clean(sei->cow[0]);
    abuf_clean(sei->cow[1]);
#endif

#ifdef SEI_WRAP_SC
    wts_flush(sei->wts);
#endif

    tbin_flush(sei->tbin);
    talloc_clean(sei->talloc);
    obuf_close(sei->obuf);

    r = cfc_check(&sei->cf[0]);
    assert (r && "control flow error");
    r = cfc_check(&sei->cf[1]);
    assert (r && "control flow error");

    r = ibuf_correct(sei->ibuf);
    assert (r == 1 && "input message modified");

    SEI_STATS_INC(ntrav);
    SEI_STATS_REPORT();
#ifdef HEAP_PROTECT
    int i;
    for (i = 0; i < abuf_size(sei->wpages); ++i) {
        uint64_t size;
        void* ptr = abuf_pop(sei->wpages, &size);
        protect_mem(ptr, size, READ);
        SEI_STATS_INC(nprotect);
    }
    abuf_clean(sei->wpages);
#endif
}

inline int
sei_getp(sei_t* sei)
{
    return sei->p;
}

inline void
sei_setp(sei_t* sei, int p)
{
    sei->p = p;
}

int
sei_shift(sei_t* sei, int handle)
{
    if (handle == -1) {
        // create new obuf and exchange; use current if first time
        if (stash_size(sei->stash) != 0) {
            // here we assume that current obuf already in stash
            sei->obuf = obuf_init(OBUF_SIZE);
        }
        // add to stash
        handle = stash_add(sei->stash, sei->obuf);
    } else {
        // exchange obuf
        sei->obuf = (obuf_t*) stash_get(sei->stash, handle);
    }

    return handle;
}

/* ----------------------------------------------------------------------------
 * memory management
 * ------------------------------------------------------------------------- */

inline void*
sei_malloc(sei_t* sei, size_t size)
{
    SEI_STATS_INC(nmalloc);
    void* ptr = talloc_malloc(sei->talloc, size);
#if defined(HEAP_PROTECT)
    // && (!defined(COW_USEHEAP) || HEAP_SIZE == HEAP_NP)
    // if heap has to be protected and
    // either we don't use heap_t or
    // we do use heap_t but it's not preallocated (HEAP_NP)
    // then we have to protect the heap whever we do a malloc

    if (sei->p == 0) {
        // we don't have to protect that for the second execution
        protect_mem(ptr, size, READ);
    }
#endif
    return ptr;
}

inline void
sei_free(sei_t* sei, void* ptr)
{
    SEI_STATS_INC(nfree);
    tbin_add(sei->tbin, ptr, sei->p);
}

void*
sei_calloc(sei_t* sei, size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
    return NULL;
}

/* ----------------------------------------------------------------------------
 * memory management outside traversal
 * ------------------------------------------------------------------------- */

void*
sei_malloc2(sei_t* sei, size_t size)
{
    return malloc(size);
}

void
sei_free2(sei_t* sei, void* ptr1, void* ptr2)
{
    assert (0 && "sei not compiled with HEAP_MODE");
}

inline void*
sei_other(sei_t* sei, void* addr)
{
    assert (0 && "sei not compiled with HEAP_MODE");
    return NULL;
}

void*
sei_memcpy2(sei_t* sei, void* dest, const void* src, size_t n)
{
    assert (0 && "sei not compiled with HEAP_MODE");
    return NULL;
}

#ifdef HEAP_PROTECT
void
sei_unprotect(sei_t* sei, void* addr, size_t size)
{
    if (sei->p == 1) {
        assert (0 && "straaaange");
    }
    if (sei->p == 0 || sei->p == -1) {
        abuf_push(sei->wpages, addr, size);
        protect_mem(addr, size, WRITE);
    }
}
#endif


/* ----------------------------------------------------------------------------
 * load and stores
 * ------------------------------------------------------------------------- */

#ifndef COW_APPEND_ONLY
#define SEI_READ(type) inline                                           \
    type sei_read_##type(sei_t* sei, const type* addr)                  \
    {                                                                   \
        DLOG3("sei_read_%s(%d) addr = %p", #type, sei->p, addr);        \
        cow_t* cow = sei->cow[sei->p];                                  \
        type value = cow_read_##type(cow, addr);                        \
        DLOG3("= %lx, %lx\n", (uint64_t) *addr, value);                 \
        return value;                                                   \
    }
SEI_READ(uint8_t)
SEI_READ(uint16_t)
SEI_READ(uint32_t)
SEI_READ(uint64_t)

#define SEI_WRITE(type) inline                                          \
    void sei_write_##type(sei_t* sei, type* addr, type value)           \
    {                                                                   \
        SEI_STATS_INC(nw##type);                                        \
        assert (sei->p == 0 || sei->p == 1);                            \
        DLOG3("sei_write_%s(%d): %p <- %llx\n", #type, sei->p,          \
              addr, (uint64_t) value);                                  \
        cow_t* cow = sei->cow[sei->p];                                  \
        cow_write_##type(cow, addr, value);                             \
    }
SEI_WRITE(uint8_t)
SEI_WRITE(uint16_t)
SEI_WRITE(uint32_t)
SEI_WRITE(uint64_t)
#else

#define SEI_READ(type) inline                                           \
    type sei_read_##type(sei_t* sei, const type* addr)                  \
    {                                                                   \
        DLOG3("sei_read_%s(%d) %p = %lx", #type, sei->p, addr,          \
              (uint64_t) *addr);                                        \
        return *addr;                                                   \
    }
SEI_READ(uint8_t)
SEI_READ(uint16_t)
SEI_READ(uint32_t)
SEI_READ(uint64_t)

#define SEI_WRITE(type) inline                                          \
    void sei_write_##type(sei_t* sei, type* addr, type value)           \
    {                                                                   \
   	    SEI_STATS_INC(nw##type);                                        \
   	    assert (sei->p == 0 || sei->p == 1);                            \
        DLOG3("sei_write_%s(%d): %p <- %llx\n", #type, sei->p,          \
              addr, (uint64_t) value);                                  \
        abuf_push_##type(sei->cow[sei->p], addr, *addr);                \
        *addr = value;                                                  \
    }
SEI_WRITE(uint8_t)
SEI_WRITE(uint16_t)
SEI_WRITE(uint32_t)
SEI_WRITE(uint64_t)
#endif

/* ----------------------------------------------------------------------------
 * output messages
 * ------------------------------------------------------------------------- */

/* sei_output_append and sei_output_done can be called from outside
 * a handler with no effect.
 *
 * sei_output_next can only be called from outside the handler.
 */
void
sei_output_append(sei_t* sei, const void* ptr, size_t size)
{
    if (sei->p == -1) return;
    obuf_push(sei->obuf, ptr, size);
}

void
sei_output_done(sei_t* sei)
{
    if (sei->p == -1) return;
    obuf_done(sei->obuf);
}

uint32_t
sei_output_next(sei_t* sei)
{
    assert (sei->p == -1);
    assert (obuf_size(sei->obuf) > 0 && "no CRC to pop");

    uint32_t crc = obuf_pop(sei->obuf);

    return crc;
}

/* ----------------------------------------------------------------------------
 * system call management
 * ------------------------------------------------------------------------- */

void*
sei_get_wts(sei_t* sei)
{
	return sei->wts;
}

#ifdef SEI_CPU_ISOLATION
/* ----------------------------------------------------------------------------
 * Rollback and non-destructive commit for SDC recovery
 * ------------------------------------------------------------------------- */

void
sei_rollback(sei_t* sei)
{
    assert(sei);
    DLOG1("[sei_rollback] Rolling back transaction\n");

    /* Step 1: Restore memory from COW buffer (abuf[0] contains old values) */
#ifdef COW_APPEND_ONLY
    abuf_restore(sei->cow[0]);
    /* Clean both buffers to prevent stale buffer state during retry */
    abuf_clean(sei->cow[0]);
    abuf_clean(sei->cow[1]);
#else
    #error "sei_rollback only supports COW_APPEND_ONLY mode"
#endif

    /* Step 2: Rollback dynamic allocations (free all talloc allocations) */
    talloc_rollback(sei->talloc);

    /* Step 3: Reset waitress queue (discard delayed system calls) */
#ifdef SEI_WRAP_SC
    wts_reset(sei->wts);
#endif

    /* Step 4: Reset sei state to beginning of transaction */
    sei->p = 0;

    /* Step 5: Reset control flags */
    cfc_reset(&sei->cf[0]);
    cfc_reset(&sei->cf[1]);

    DLOG1("[sei_rollback] Rollback complete\n");
}

int
sei_try_commit(sei_t* sei)
{
    int core_id = sched_getcpu();

    assert(sei);
    assert(sei->p == 1 && "must be in p=1 state before commit");

    /* Non-destructive commit: verify DMR but don't modify state yet */

    int result = 1;

    /* Rewind buffers BEFORE comparison to ensure poped == 0 */
    abuf_rewind(sei->cow[0]);
    abuf_rewind(sei->cow[1]);

    /* CRITICAL: Swap cow[1] to extract NEW_1 values for comparison.
     * At this point:
     *   cow[0]->wvalue = NEW_0 (p=0 results, saved by abuf_swap in sei_switch)
     *   cow[1]->wvalue = OLD (recorded by sei_write during p=1)
     *   Memory[X] = NEW_1 (written by p=1)
     *
     * After abuf_swap(cow[1]):
     *   cow[0]->wvalue = NEW_0
     *   cow[1]->wvalue = NEW_1 (extracted from memory)
     *   Memory[X] = OLD (restored for potential rollback)
     */
    abuf_swap(sei->cow[1]);

    /* Check COW buffer consistency (DMR verification: NEW_0 vs NEW_1) */
#ifdef COW_APPEND_ONLY
    if (!abuf_try_cmp(sei->cow[0], sei->cow[1])) {
        DLOG1("[sei_try_commit] COW buffer mismatch detected\n");
        result = 0;  /* SDC detected */
    }

    /* Restore cow[1] to original state: Memory = NEW_1, cow[1] = OLD
     * This must be done regardless of comparison result, because:
     * - If SDC detected: sei_rollback() needs cow[1] in original state
     * - If SDC not detected: sei_commit() needs cow[1] in original state */
    abuf_swap(sei->cow[1]);

    if (!result) {
        return 0;
    }
#else
    #error "sei_try_commit only supports COW_APPEND_ONLY mode"
#endif

    /* Check control flags consistency */
    if (cfc_amog(&sei->cf[0]) != 0) {
        DLOG1("[sei_try_commit] Control flag mismatch detected\n");
        return 0;  /* SDC detected */
    }

    /* Check talloc consistency */
    if (sei->talloc->size[0] != sei->talloc->size[1]) {
        DLOG1("[sei_try_commit] Talloc allocation count mismatch\n");
        return 0;  /* SDC detected */
    }

#ifdef SEI_WRAP_SC
    /* Check waitress consistency */
    if (sei->wts->nitems[0] != sei->wts->nitems[1]) {
        DLOG1("[sei_try_commit] Waitress call count mismatch\n");
        return 0;  /* SDC detected */
    }
#endif

    /* All checks passed - verification successful */
    return 1;  /* Success */
}

#endif /* SEI_CPU_ISOLATION */
