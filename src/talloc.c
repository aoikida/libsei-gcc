/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>

/* -----------------------------------------------------------------------------
 * types, data structures and definitions
 * -------------------------------------------------------------------------- */

#define TALLOC_MAX_ALLOCS 100

#include "talloc.h"
#include "heap.h"
#ifdef ASCO_STACK_INFO
#include "sinfo.h"
#endif

typedef struct {
    void* addr;
#ifdef ASCO_STACK_INFO
    sinfo_t* sinfo[2];
#endif
} talloc_allocation_t;

struct talloc {
    int p;
    heap_t* heap;
    talloc_allocation_t allocations[TALLOC_MAX_ALLOCS];
    size_t size[2];
};

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

talloc_t*
talloc_init(heap_t* heap)
{
    talloc_t* talloc = (talloc_t*) malloc(sizeof(talloc_t));
    assert (talloc && "out of memory");
    bzero(talloc, sizeof(talloc_t));
    talloc->p = 0;

    talloc->size[0] = 0;
    talloc->size[1] = 0;

    talloc->heap = heap;

    return talloc;
}

void
talloc_fini(talloc_t* talloc)
{
    assert (talloc);
    free(talloc);
}

/* -----------------------------------------------------------------------------
 * interface methods
 * -------------------------------------------------------------------------- */

inline void*
talloc_malloc(talloc_t* talloc, size_t size)
{
    assert (talloc);
    talloc_allocation_t* a = NULL;

    if (talloc->p == 0) {
        assert (talloc->size[0] + 1 < TALLOC_MAX_ALLOCS && "cant allocate");
        a = &talloc->allocations[talloc->size[0]++];
        if (talloc->heap)
            a->addr = heap_malloc(talloc->heap, size);
        else
            a->addr = malloc(size);
        assert (a->addr && "out of memory");
#ifdef ASCO_STACK_INFO
        a->sinfo[0] = sinfo_init(a->addr);
#endif
    } else {
        assert (talloc->p == 1);
        a = &talloc->allocations[talloc->size[1]++];
#ifdef ASCO_STACK_INFO
        a->sinfo[1] = sinfo_init(a->addr);
#endif
    }
    return a->addr;
}

void
talloc_switch(talloc_t* talloc)
{
   assert (talloc);
   assert (talloc->p == 0);
   assert (talloc->size[1] == 0);
   talloc->p = 1;
}


void
talloc_clean(talloc_t* talloc)
{
   assert (talloc);
   assert (talloc->p == 1);
   assert (talloc->size[0] == talloc->size[1]
           && "number of allocations in traversals differ");

#ifdef ASCO_STACK_INFO
   int i;
   for (i = 0; i < talloc->size[0]; ++i) {
       talloc_allocation_t* a = &talloc->allocations[i];
       sinfo_fini(a->sinfo[0]);
       sinfo_fini(a->sinfo[1]);
       a->sinfo[0] = a->sinfo[1] = NULL;
   }
#endif
   talloc->p = 0;
   talloc->size[0] = talloc->size[1] = 0;
}
