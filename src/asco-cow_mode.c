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
#include "cow.h"
#include "tbin.h"
#include "talloc.h"

struct asco {
    int       p;       /* the actual process (0 or 1) */
    heap_t*   heap;    /* optional heap               */
    cow_t*    cow[2];  /* a copy-on-write buffer      */
    tbin_t*   tbin;    /* trash bin for delayed frees */
    talloc_t* talloc;  /* traversal allocarot         */
};

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

asco_t*
asco_init()
{
    asco_t* asco = (asco_t*) malloc(sizeof(asco_t));
    assert(asco);
    asco->cow[0] = cow_init(100000);
    asco->cow[1] = cow_init(100000);

#ifdef COW_USEHEAP
    asco->heap   = heap_init(HEAP_1GB);
#else
    asco->heap   = NULL;
#endif
    asco->tbin   = tbin_init(100, asco->heap);
    asco->talloc = talloc_init(asco->heap);

    asco->p = -1;

    DLOG3("asco_init addr: %p (heap = {%p})\n", asco, asco->heap);

    return asco;
}

void
asco_fini(asco_t* asco)
{
    assert(asco);
    cow_fini(asco->cow[0]);
    cow_fini(asco->cow[1]);

    heap_fini(asco->heap);
    tbin_fini(asco->tbin);
    talloc_fini(asco->talloc);
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
    talloc_switch(asco->talloc);
#ifdef COWBACK
    cow_swap(asco->cow[0]);
#endif
}

void
asco_commit(asco_t* asco)
{
    DLOG2("COMMIT: %d\n", asco->p);
    asco->p = -1;

    cow_show(asco->cow[0]);
    cow_show(asco->cow[1]);
    cow_apply_cmp(asco->cow[0], asco->cow[1]);
    tbin_flush(asco->tbin);
    talloc_clean(asco->talloc);
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
    return talloc_malloc(asco->talloc, size);
}

inline void
asco_free(asco_t* asco, void* ptr)
{
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

/* -----------------------------------------------------------------------------
 * load and stores
 * -------------------------------------------------------------------------- */

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
