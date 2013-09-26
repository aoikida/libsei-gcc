/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <asco.h>
#include "debug.h"
#include "fail.h"

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

#include "heap.h"
#include "cow.h"

struct asco {
    int       p;       /* the actual process (0 or 1) */
    heap_t*   heap[2]; /* the heap of each process    */
    cow_t*    cow[2];  /* a copy-on-write buffer      */
};

#if __WORDSIZE == 64
typedef uint64_t addr_t;
#else
typedef uint32_t addr_t;
#endif

#ifndef HEAP_SIZE
#define HEAP_SIZE HEAP_500MB
#endif

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

asco_t*
asco_init()
{
    asco_t* asco = (asco_t*) malloc(sizeof(asco_t));
    assert(asco);


    asco->heap[0] = heap_init(HEAP_SIZE);
    asco->heap[1] = heap_init(HEAP_SIZE);

    asco->cow[0] = cow_init(asco->heap[0], COW_SIZE);
    asco->cow[1] = cow_init(asco->heap[1], COW_SIZE);
    asco->p = -1;

    DLOG3("asco_init addr: %p (heap = {%p, %p})\n", asco,
          asco->heap[0], asco->heap[1]);

    return asco;
}

void
asco_fini(asco_t* asco)
{
    assert(asco);
    cow_fini(asco->cow[0]);
    cow_fini(asco->cow[1]);
    heap_fini(asco->heap[0]);
    heap_fini(asco->heap[1]);
}


/* -----------------------------------------------------------------------------
 * traversal control
 * -------------------------------------------------------------------------- */

void
asco_begin(asco_t* asco)
{
    if (asco->p == -1) {
        DLOG2("First execution\n");
        asco->p = 0;
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
}

void
asco_commit(asco_t* asco)
{
    DLOG2("COMMIT: %d\n", asco->p);
    asco->p = -1;

    cow_show(asco->cow[0]);
    cow_show(asco->cow[1]);
    cow_apply_heap(asco->cow[0], asco->cow[1]);
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
    void* ptr = heap_malloc(asco->heap[asco->p], size);
    DLOG3("asco_malloc addr: %p (size = %"PRIu64", p = %d)\n", ptr, (uint64_t)size, asco->p);
    return ptr;
}

inline void
asco_free(asco_t* asco, void* ptr)
{
    DLOG3("asco_free addr: %p\n", ptr);
    assert (heap_in(asco->heap[asco->p], ptr));
    heap_free(asco->heap[asco->p], ptr);
}

void*
asco_calloc(asco_t* asco, size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
    return 0;
}

/* -----------------------------------------------------------------------------
 * memory management outside traversal
 * -------------------------------------------------------------------------- */

void*
asco_malloc2(asco_t* asco, size_t size)
{
    assert (asco);
    assert (asco->p == -1 && "should not be called in a traversal");
    asco->p = 0;
    void* ptr1 = asco_malloc(asco, size);
    assert (ptr1);
    asco->p = 1;
#ifndef NDEBUG
    void* ptr2 = asco_malloc(asco, size);
#else
    asco_malloc(asco, size);
#endif
    assert (ptr2);
    asco->p = -1;
    DLOG3("asco_malloc2 addrs:(%p, %p)\n", ptr1, ptr2);
    return ptr1;
}

void
asco_free2(asco_t* asco, void* ptr1, void* ptr2)
{
    assert (asco);
    assert (asco->p == -1 && "should not be called in a traversal");
    DLOG3("asco_free2 addrs:(%p, %p)\n", ptr1, ptr2);
    heap_free(asco->heap[0], ptr1);
    heap_free(asco->heap[1], ptr2);
}

inline void*
asco_other(asco_t* asco, void* addr)
{
    assert (asco->p == 0 || asco->p == 1);
    if (heap_in(asco->heap[asco->p], addr)) {
        size_t rel = heap_rel(asco->heap[asco->p], (void*) addr);
        void* other = (void*) heap_get(asco->heap[1-asco->p], rel);
        return other;
    } else {
        assert (heap_in(asco->heap[1-asco->p], addr) && "address in no heap");
        size_t rel = heap_rel(asco->heap[1-asco->p], (void*) addr);
        void* other = (void*) heap_get(asco->heap[asco->p], rel);
        return other;
    }
}

void*
asco_memcpy2(asco_t* asco, void* dest, const void* src, size_t n)
{
    assert (asco);
    assert (asco->p == -1 && "should not be called in a traversal");

    asco->p = 0;
    memcpy(dest, src, n);
    memcpy(asco_other(asco, dest), src, n);
    asco->p = -1;
    DLOG3("asco_memcpy2 addrs:(%p, %p)\n", dest, asco_other(asco, dest));
    return dest;
}

/* -----------------------------------------------------------------------------
 * load and stores
 * -------------------------------------------------------------------------- */

#define ASCO_READ(type) inline                                          \
    type asco_read_##type(asco_t* asco, const type* addr)               \
    {                                                                   \
        assert (asco->p == 0 || asco->p == 1);                          \
        DLOG3("asco_read_%s addr = %p", #type, addr);                   \
        if (heap_in(asco->heap[1-asco->p], (void*) addr)) {             \
            ASCO_FAIL("ERROR, reading from other heap %p\n", addr);     \
            assert (0);                                                 \
        }                                                               \
        if (!heap_in(asco->heap[asco->p], (void*) addr)) {              \
            DLOG3("(not in heap) = %"PRIu64" x%"PRIx64" \n", (uint64_t) *addr,      \
                  (uint64_t) *addr);                                    \
            /* ASCO_FAIL("ERROR, reading from no heap"); */             \
            return *addr;                                               \
        }                                                               \
        DLOG3("(in heap) = %"PRId64" x%"PRIx64"\n", (uint64_t) *addr,               \
              (uint64_t) *addr);                                        \
        size_t rel = heap_rel(asco->heap[asco->p], (void*) addr);       \
        type* addr2 = (type*) heap_get(asco->heap[1-asco->p], rel);     \
        DLOG2("checking rel = %"PRId64" (%"PRIu64" %"PRId64")\n", (int64_t)rel,                    \
                   (uint64_t)*addr, (int64_t)*addr2);                  \
        if (*addr != *addr2) {                                          \
            /* could the variable be a pointer? */                      \
            if (sizeof(type) == sizeof(addr_t)) {                     \
                type* ptr1 = (type*) (addr_t) *addr;                  \
                type* ptr2 = (type*) (addr_t) *addr2;                 \
                size_t rel1 = heap_rel(asco->heap[asco->p], ptr1);      \
                size_t rel2 = heap_rel(asco->heap[1-asco->p], ptr2);    \
                fail_ifn(rel1 == rel2, "error mem check");              \
            } else {                                                    \
                fail_ifn(0, "error mem check");                         \
            }                                                           \
        }                                                               \
        if (0 && asco->p == 1)                                          \
            return *addr;                                               \
        cow_t* cow = asco->cow[asco->p];                                \
        return cow_read_##type(cow, addr);                              \
    }
ASCO_READ(uint8_t)
ASCO_READ(uint16_t)
ASCO_READ(uint32_t)
ASCO_READ(uint64_t)

#define ASCO_WRITE(type) inline                                         \
    void asco_write_##type(asco_t* asco, type* addr, type value)        \
    {                                                                   \
        assert (asco->p == 0 || asco->p == 1);                          \
        DLOG3("asco_write_%s: %p <- %"PRId64" x%"PRIx64"\n", #type, addr,         \
              (int64_t) value, (int64_t) value);                      \
        if (!heap_in(asco->heap[asco->p], addr)) {                      \
            if (heap_in(asco->heap[1-asco->p], addr)) {                 \
                printf("ERROR, writing on other heap %p\n", addr);      \
                assert (0);                                             \
            }                                                           \
            *addr = value;                                              \
            return;                                                     \
        }                                                               \
        cow_t* cow = asco->cow[asco->p];                                \
        cow_write_##type(cow, addr, value);                             \
    }
ASCO_WRITE(uint8_t)
ASCO_WRITE(uint16_t)
ASCO_WRITE(uint32_t)
ASCO_WRITE(uint64_t)
