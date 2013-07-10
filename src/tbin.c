/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

/* -----------------------------------------------------------------------------
 * types, data structures and definitions
 * -------------------------------------------------------------------------- */

#include "tbin.h"
#include "heap.h"
#ifdef ASCO_STACK_INFO
#include "sinfo.h"
#endif

typedef struct {
    void* ptr[2];
#ifdef ASCO_STACK_INFO
    sinfo_t* sinfo[2];
#endif
} tbin_item_t;

struct tbin {
    int max_items;      // maximum number of items
    int nitems[2];      // actual number of items
    tbin_item_t* items; // array of items
    heap_t* heap;       // heap
};

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

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
    tbin->nitems[0] = tbin->nitems[1] = 0;

    tbin->heap = heap;

    return tbin;
}

void
tbin_fini(tbin_t* tbin)
{
    assert (tbin);
    free(tbin);
}

/* -----------------------------------------------------------------------------
 * interface methods
 * -------------------------------------------------------------------------- */

inline void
tbin_add(tbin_t* tbin, void* ptr, int p)
{
    assert (tbin);
    assert ((p == 0 || p == 1) && "invalid p");
    assert (tbin->nitems[p] + 1 <= tbin->max_items && "cannot add item");
    tbin_item_t* it = &tbin->items[tbin->nitems[p]++];
    it->ptr[p] = ptr;

#ifdef ASCO_STACK_INFO
    assert (it->sinfo[p] == NULL);
    it->sinfo[p] = sinfo_init(ptr);
#endif
}

void
tbin_flush(tbin_t* tbin)
{
    assert (tbin);
    assert (tbin->nitems[0] == tbin->nitems[1] && "number of items differ");
    tbin_item_t* it = &tbin->items[0];
    int i = 0;
    for (; i < tbin->nitems[0]; ++i, ++it) {
        assert (it->ptr[0] && it->ptr[1] && "only one pointers passed");
        assert (it->ptr[0] == it->ptr[1] && "pointers differ");
        if (tbin->heap && heap_in(tbin->heap, it->ptr[0]))
            heap_free(tbin->heap, it->ptr[0]);
        else
            free(it->ptr[0]);
        it->ptr[0] = it->ptr[1] = NULL;
#ifdef ASCO_STACK_INFO
        sinfo_fini(it->sinfo[0]);
        sinfo_fini(it->sinfo[1]);
        it->sinfo[0] = it->sinfo[1] = NULL;
#endif
    }
    tbin->nitems[0] = tbin->nitems[1] = 0;
}
