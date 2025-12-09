/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "fail.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */

#include "tbin.h"
#include "heap.h"
#ifdef SEI_STACK_INFO
#include "sinfo.h"
#endif

/* N-way DMR redundancy configuration */
#ifndef SEI_DMR_REDUNDANCY
#define SEI_DMR_REDUNDANCY 2
#endif

typedef struct {
    void* ptr[SEI_DMR_REDUNDANCY];
#ifdef SEI_STACK_INFO
    sinfo_t* sinfo[SEI_DMR_REDUNDANCY];
#endif
} tbin_item_t;

struct tbin {
    int max_items;                     // maximum number of items
    int nitems[SEI_DMR_REDUNDANCY];    // actual number of items per phase
    tbin_item_t* items;                // array of items
    heap_t* heap;                      // heap
};

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

tbin_t*
tbin_init(int max_items, heap_t* heap)
{
    assert (max_items > 0 && "invalid maximal number of items");
    tbin_t* tbin = (tbin_t*) malloc(sizeof(tbin_t));
    assert (tbin && "out of memory");
    tbin->max_items = max_items;
    tbin->items     = (tbin_item_t*) malloc(sizeof(tbin_item_t)*max_items);
    assert (tbin->items && "out of memory");
    bzero(tbin->items, sizeof(tbin_item_t)*max_items);

    /* Initialize all phase counters */
    for (int i = 0; i < SEI_DMR_REDUNDANCY; i++) {
        tbin->nitems[i] = 0;
    }

    tbin->heap = heap;

    return tbin;
}

void
tbin_fini(tbin_t* tbin)
{
    assert (tbin);
    free(tbin);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

inline void
tbin_add(tbin_t* tbin, void* ptr, int p)
{
    assert (tbin);
    assert (p >= 0 && p < SEI_DMR_REDUNDANCY && "invalid p");
    assert (tbin->nitems[p] + 1 <= tbin->max_items && "cannot add item");
    tbin_item_t* it = &tbin->items[tbin->nitems[p]++];
    it->ptr[p] = ptr;

#ifdef SEI_STACK_INFO
    assert (it->sinfo[p] == NULL);
    it->sinfo[p] = sinfo_init(ptr);
#endif
}

/* Pre-check for tbin_flush without freeing memory
 * Returns: 1 if can flush safely, 0 if mismatch detected */
inline int
tbin_can_flush(tbin_t* tbin)
{
    assert(tbin);

    /* N-way verification: all phases must have same item count */
    int expected_count = tbin->nitems[0];
    for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
        if (tbin->nitems[p] != expected_count)
            return 0;
    }

    /* N-way verification: all pointers must match for each item */
    tbin_item_t* it = &tbin->items[0];
    for (int i = 0; i < expected_count; ++i, ++it) {
        /* Check all phases have valid pointers */
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            if (!it->ptr[p])
                return 0;
        }
        /* Compare all phases against Phase 0 (reference) */
        for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
            if (it->ptr[0] != it->ptr[p])
                return 0;
        }
    }
    return 1;
}

inline void
tbin_flush(tbin_t* tbin)
{
    assert (tbin);

    /* N-way verification: all phases must have same item count */
    int expected_count = tbin->nitems[0];
    for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
        fail_ifn(tbin->nitems[p] == expected_count,
                 "number of items differ across phases");
    }

    /* Process and verify all items */
    tbin_item_t* it = &tbin->items[0];
    for (int i = 0; i < expected_count; ++i, ++it) {
        /* N-way verification: all pointers must be valid */
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            fail_ifn(it->ptr[p], "null pointer in phase");
        }

        /* N-way verification: all pointers must match Phase 0 */
        for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
            fail_ifn(it->ptr[0] == it->ptr[p], "pointers differ across phases");
        }

        /* Free memory once (using Phase 0 pointer as reference) */
        if (tbin->heap && heap_in(tbin->heap, it->ptr[0]))
            heap_free(tbin->heap, it->ptr[0]);
        else
            free(it->ptr[0]);

        /* Clear all phase pointers */
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            it->ptr[p] = NULL;
        }

#ifdef SEI_STACK_INFO
        /* Finalize and clear all sinfo entries */
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            sinfo_fini(it->sinfo[p]);
            it->sinfo[p] = NULL;
        }
#endif
    }

    /* Reset all phase counters */
    for (int i = 0; i < SEI_DMR_REDUNDANCY; i++) {
        tbin->nitems[i] = 0;
    }
}
