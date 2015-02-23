/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sei.h>
#include "debug.h"
#include "fail.h"
#include "config.h"

/* ----------------------------------------------------------------------------
 * types and data structures
 * ------------------------------------------------------------------------- */

#include "heap.h"
#include "cow.h"
#include "cfc.h"

struct sei {
    int       p;       /* the actual process (0 or 1) */
    heap_t*   heap[2]; /* the heap of each process    */
    cow_t*    cow[2];  /* a copy-on-write buffer      */
    cfc_t     cf[2];   /* control flags               */
};

#if __WORDSIZE == 64
typedef uint64_t addr_t;
#else
typedef uint32_t addr_t;
#endif

#ifndef HEAP_SIZE
#define HEAP_SIZE HEAP_500MB
#endif

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

sei_t*
sei_init()
{
    sei_t* sei = (sei_t*) malloc(sizeof(sei_t));
    assert(sei);


    sei->heap[0] = heap_init(HEAP_SIZE);
    sei->heap[1] = heap_init(HEAP_SIZE);

    sei->cow[0] = cow_init(sei->heap[0], COW_SIZE);
    sei->cow[1] = cow_init(sei->heap[1], COW_SIZE);
    sei->p = -1;

    DLOG3("sei_init addr: %p (heap = {%p, %p})\n", sei,
          sei->heap[0], sei->heap[1]);

    return sei;
}

void
sei_fini(sei_t* sei)
{
    assert(sei);
    cow_fini(sei->cow[0]);
    cow_fini(sei->cow[1]);
    heap_fini(sei->heap[0]);
    heap_fini(sei->heap[1]);
}


/* ----------------------------------------------------------------------------
 * traversal control
 * ------------------------------------------------------------------------- */

void
sei_begin(sei_t* sei)
{
    if (sei->p == -1) {
        DLOG2("First execution\n");
        sei->p = 0;
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

    cfc_alog(&sei->cf[0]);
    int r = cfc_amog(&sei->cf[0]);
    fail_ifn(r, "control flow error");

    DLOG2("Switched: %d\n", sei->p);
}

void
sei_commit(sei_t* sei)
{
    DLOG2("COMMIT: %d\n", sei->p);
    sei->p = -1;

    int r = cfc_amog(&sei->cf[1]);
    fail_ifn(r, "control flow error");
    cfc_alog(&sei->cf[1]);

    cow_show(sei->cow[0]);
    cow_show(sei->cow[1]);
    cow_apply_heap(sei->cow[0], sei->cow[1]);

    r = cfc_check(&sei->cf[0]);
    fail_ifn(r,"control flow error");
    r = cfc_check(&sei->cf[1]);
    fail_ifn(r, "control flow error");
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

/* ----------------------------------------------------------------------------
 * memory management
 * ------------------------------------------------------------------------- */

inline void*
sei_malloc(sei_t* sei, size_t size)
{
    void* ptr = heap_malloc(sei->heap[sei->p], size);
    DLOG3("sei_malloc addr: %p (size = %"PRIu64", p = %d)\n", ptr,
          (uint64_t)size, sei->p);
    return ptr;
}

inline void
sei_free(sei_t* sei, void* ptr)
{
    DLOG3("sei_free addr: %p\n", ptr);
    assert (heap_in(sei->heap[sei->p], ptr));
    heap_free(sei->heap[sei->p], ptr);
}

void*
sei_calloc(sei_t* sei, size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
    return 0;
}

/* ----------------------------------------------------------------------------
 * memory management outside traversal
 * ------------------------------------------------------------------------- */

void*
sei_malloc2(sei_t* sei, size_t size)
{
    assert (sei);
    assert (sei->p == -1 && "should not be called in a traversal");
    sei->p = 0;
    void* ptr1 = sei_malloc(sei, size);
    assert (ptr1);
    sei->p = 1;
#ifndef NDEBUG
    void* ptr2 = sei_malloc(sei, size);
#else
    sei_malloc(sei, size);
#endif
    assert (ptr2);
    sei->p = -1;
    DLOG3("sei_malloc2 addrs:(%p, %p)\n", ptr1, ptr2);
    return ptr1;
}

void
sei_free2(sei_t* sei, void* ptr1, void* ptr2)
{
    assert (sei);
    assert (sei->p == -1 && "should not be called in a traversal");
    DLOG3("sei_free2 addrs:(%p, %p)\n", ptr1, ptr2);
    heap_free(sei->heap[0], ptr1);
    heap_free(sei->heap[1], ptr2);
}

inline void*
sei_other(sei_t* sei, void* addr)
{
    assert (sei->p == 0 || sei->p == 1);
    if (heap_in(sei->heap[sei->p], addr)) {
        size_t rel = heap_rel(sei->heap[sei->p], (void*) addr);
        void* other = (void*) heap_get(sei->heap[1-sei->p], rel);
        return other;
    } else {
        assert (heap_in(sei->heap[1-sei->p], addr) && "address in no heap");
        size_t rel = heap_rel(sei->heap[1-sei->p], (void*) addr);
        void* other = (void*) heap_get(sei->heap[sei->p], rel);
        return other;
    }
}

void*
sei_memcpy2(sei_t* sei, void* dest, const void* src, size_t n)
{
    assert (sei);
    assert (sei->p == -1 && "should not be called in a traversal");

    sei->p = 0;
    memcpy(dest, src, n);
    memcpy(sei_other(sei, dest), src, n);
    sei->p = -1;
    DLOG3("sei_memcpy2 addrs:(%p, %p)\n", dest, sei_other(sei, dest));
    return dest;
}

/* ----------------------------------------------------------------------------
 * load and stores
 * ------------------------------------------------------------------------- */

#define ASCO_READ(type) inline                                          \
    type sei_read_##type(sei_t* sei, const type* addr)                  \
    {                                                                   \
        assert (sei->p == 0 || sei->p == 1);                            \
        DLOG3("sei_read_%s addr = %p", #type, addr);                    \
        if (heap_in(sei->heap[1-sei->p], (void*) addr)) {               \
            ASCO_FAIL("ERROR, reading from other heap %p\n", addr);     \
            assert (0);                                                 \
        }                                                               \
        if (!heap_in(sei->heap[sei->p], (void*) addr)) {                \
            DLOG3("(not in heap) = %"PRIu64" x%"PRIx64" \n",            \
                  (uint64_t) *addr, (uint64_t) *addr);                  \
            /* ASCO_FAIL("ERROR, reading from no heap"); */             \
            return *addr;                                               \
        }                                                               \
        DLOG3("(in heap) = %"PRId64" x%"PRIx64"\n",                     \
              (uint64_t) *addr, (uint64_t) *addr);                      \
        size_t rel = heap_rel(sei->heap[sei->p], (void*) addr);         \
        type* addr2 = (type*) heap_get(sei->heap[1-sei->p], rel);       \
        DLOG2("checking rel = %"PRId64" (%"PRIu64" %"PRId64")\n",       \
              (int64_t) rel, (uint64_t)*addr, (int64_t)*addr2);         \
        if (*addr != *addr2) {                                          \
            /* could the variable be a pointer? */                      \
            if (sizeof(type) == sizeof(addr_t)) {                       \
                type* ptr1 = (type*) (addr_t) *addr;                    \
                type* ptr2 = (type*) (addr_t) *addr2;                   \
                size_t rel1 = heap_rel(sei->heap[sei->p], ptr1);        \
                size_t rel2 = heap_rel(sei->heap[1-sei->p], ptr2);      \
                fail_ifn(rel1 == rel2, "error mem check");              \
            } else {                                                    \
                fail_ifn(0, "error mem check");                         \
            }                                                           \
        }                                                               \
        if (0 && sei->p == 1)                                           \
            return *addr;                                               \
        cow_t* cow = sei->cow[sei->p];                                  \
        return cow_read_##type(cow, addr);                              \
    }
ASCO_READ(uint8_t)
ASCO_READ(uint16_t)
ASCO_READ(uint32_t)
ASCO_READ(uint64_t)

#define ASCO_WRITE(type) inline                                         \
    void sei_write_##type(sei_t* sei, type* addr, type value)           \
    {                                                                   \
        assert (sei->p == 0 || sei->p == 1);                            \
        DLOG3("sei_write_%s: %p <- %"PRId64" x%"PRIx64"\n", #type,      \
              addr, (int64_t) value, (int64_t) value);                  \
        if (!heap_in(sei->heap[sei->p], addr)) {                        \
            if (heap_in(sei->heap[1-sei->p], addr)) {                   \
                fprintf(stderr, "ERROR, writing on other heap %p\n",    \
                        addr);                                          \
                /* TODO: should be fail_ifn here */                     \
                assert (0);                                             \
            }                                                           \
            *addr = value;                                              \
            return;                                                     \
        }                                                               \
        cow_t* cow = sei->cow[sei->p];                                  \
        cow_write_##type(cow, addr, value);                             \
    }
ASCO_WRITE(uint8_t)
ASCO_WRITE(uint16_t)
ASCO_WRITE(uint32_t)
ASCO_WRITE(uint64_t)
