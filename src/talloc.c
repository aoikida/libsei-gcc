/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include "fail.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */


#include "config.h"
#include "talloc.h"
#include "heap.h"
#ifdef SEI_STACK_INFO
#include "sinfo.h"
#endif

/* N-way DMR redundancy configuration */
#ifndef SEI_DMR_REDUNDANCY
#define SEI_DMR_REDUNDANCY 2
#endif

typedef struct {
    void* addr;
    size_t size;  /* Size of allocation for range checking */
#ifdef SEI_STACK_INFO
    sinfo_t* sinfo[SEI_DMR_REDUNDANCY];
#endif
} talloc_allocation_t;

struct talloc {
    int p;
    int redundancy_level;  /* N-way redundancy level for this transaction */
    heap_t* heap;
    talloc_allocation_t allocations[TALLOC_MAX_ALLOCS];
    size_t size[SEI_DMR_REDUNDANCY];  /* allocation count for each phase */
};

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

talloc_t*
talloc_init(heap_t* heap)
{
    talloc_t* talloc = (talloc_t*) malloc(sizeof(talloc_t));
    assert (talloc && "out of memory");
    bzero(talloc, sizeof(talloc_t));
    talloc->p = 0;
    talloc->redundancy_level = SEI_DMR_REDUNDANCY;  /* Initialize to compile-time default */

    for (int i = 0; i < SEI_DMR_REDUNDANCY; i++) {
        talloc->size[i] = 0;
    }

    talloc->heap = heap;

    return talloc;
}

void
talloc_fini(talloc_t* talloc)
{
    assert (talloc);
    free(talloc);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

inline void*
talloc_malloc(talloc_t* talloc, size_t size)
{
    assert (talloc);
    assert (talloc->p >= 0 && talloc->p < SEI_DMR_REDUNDANCY);
    talloc_allocation_t* a = NULL;

    if (talloc->p == 0) {
        /* Phase 0: allocate new memory */
        assert (talloc->size[0] + 1 < TALLOC_MAX_ALLOCS && "cant allocate");
        a = &talloc->allocations[talloc->size[0]++];
        if (talloc->heap)
            a->addr = heap_malloc(talloc->heap, size);
        else
            a->addr = malloc(size);
        assert (a->addr && "out of memory");
        a->size = size;  /* Record size for range checking */
#ifdef SEI_STACK_INFO
        a->sinfo[0] = sinfo_init(a->addr);
#endif
    } else {
        /* Phase 1..N-1: reuse existing allocation */
        assert (talloc->p > 0 && talloc->p < SEI_DMR_REDUNDANCY);
        a = &talloc->allocations[talloc->size[talloc->p]++];
#ifdef SEI_STACK_INFO
        a->sinfo[talloc->p] = sinfo_init(a->addr);
#endif
    }
    return a->addr;
}

inline void
talloc_switch(talloc_t* talloc)
{
   assert (talloc);
   assert (talloc->p >= 0 && talloc->p < talloc->redundancy_level - 1);
   int next_phase = talloc->p + 1;
   assert (talloc->size[next_phase] == 0);
   talloc->p = next_phase;
}


inline void
talloc_clean(talloc_t* talloc)
{
   assert (talloc);
   int redundancy_level = talloc->redundancy_level;
   assert (talloc->p == redundancy_level - 1 && "must be in final phase");

   /* N-way verification: all phases must have same allocation count */
   size_t expected_size = talloc->size[0];
   for (int i = 1; i < redundancy_level; i++) {
       if (talloc->size[i] != expected_size) {
           fprintf(stderr, "[libsei] Talloc size mismatch: phase0=%zu, phase%d=%zu\n",
                   expected_size, i, talloc->size[i]);
           fail_ifn(0, "number of allocations in traversals differ");
       }
   }

#ifdef SEI_STACK_INFO
   int i;
   for (i = 0; i < talloc->size[0]; ++i) {
       talloc_allocation_t* a = &talloc->allocations[i];
       for (int p = 0; p < redundancy_level; p++) {
           sinfo_fini(a->sinfo[p]);
           a->sinfo[p] = NULL;
       }
   }
#endif

   /* Reset all phase counters */
   talloc->p = 0;
   for (int i = 0; i < redundancy_level; i++) {
       talloc->size[i] = 0;
   }
}

#ifdef SEI_CPU_ISOLATION
/* ----------------------------------------------------------------------------
 * Non-destructive verification for CPU isolation
 * ------------------------------------------------------------------------- */

inline int
talloc_can_commit(talloc_t* talloc)
{
    assert(talloc);
    int redundancy_level = talloc->redundancy_level;

    /* N-way verification: check if all phases have same allocation count */
    size_t expected_size = talloc->size[0];
    for (int i = 1; i < redundancy_level; i++) {
        if (talloc->size[i] != expected_size) {
            return 0;  /* Mismatch detected */
        }
    }
    return 1;  /* All phases match */
}

/* ----------------------------------------------------------------------------
 * Rollback: free all allocations and reset state
 * ------------------------------------------------------------------------- */

void
talloc_rollback(talloc_t* talloc)
{
    assert(talloc);
    int redundancy_level = talloc->redundancy_level;

    /* Free all allocations made during all phases */
    for (size_t i = 0; i < talloc->size[0]; i++) {
        talloc_allocation_t* a = &talloc->allocations[i];
        if (a->addr) {
            if (talloc->heap) {
                heap_free(talloc->heap, a->addr);
            } else {
                free(a->addr);
            }
            a->addr = NULL;
        }
#ifdef SEI_STACK_INFO
        for (int p = 0; p < redundancy_level; p++) {
            if (a->sinfo[p]) {
                sinfo_fini(a->sinfo[p]);
                a->sinfo[p] = NULL;
            }
        }
#endif
    }

    /* Reset talloc state to initial values */
    talloc->p = 0;
    for (int i = 0; i < redundancy_level; i++) {
        talloc->size[i] = 0;
    }
}

heap_t*
talloc_get_heap(talloc_t* talloc)
{
    assert(talloc);
    return talloc->heap;
}

/* Check if an address falls within any talloc allocation range.
 * Returns 1 if addr is within [allocation.addr, allocation.addr + size),
 * 0 otherwise. Used by abuf_restore_filtered() to skip heap memory. */
int
talloc_addr_in_range(talloc_t* talloc, void* addr)
{
    if (!talloc) return 0;

    for (size_t i = 0; i < talloc->size[0]; i++) {
        talloc_allocation_t* a = &talloc->allocations[i];
        if (a->addr) {
            char* start = (char*)a->addr;
            char* end = start + a->size;
            if ((char*)addr >= start && (char*)addr < end) {
                return 1;
            }
        }
    }
    return 0;
}

#endif /* SEI_CPU_ISOLATION */
