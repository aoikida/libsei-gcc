/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include "stash.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */

struct stash {
    void** array;
    int c;
    int max_items;
};

#define INITIAL_SIZE 100

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

stash_t*
stash_init()
{
    int max_items = INITIAL_SIZE;
    stash_t* stash = (stash_t*) malloc(sizeof(stash_t));
    assert (stash);
    stash->array = (void**) malloc(sizeof(void*) * max_items);
    assert (stash->array);
    bzero(stash->array, sizeof(void*) * max_items);
    stash->max_items = max_items;
    stash->c = 0;

    return stash;
}

void
stash_fini(stash_t* stash)
{
    assert (stash);
    assert (stash->array);
    free(stash->array);
    free(stash);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

int
stash_add(stash_t* stash, void* item)
{
    assert (stash);
    // TODO: if greater than max_items, extend data structure
    assert (stash->c < stash->max_items);

    stash->array[stash->c] = item;
    return stash->c++;
}

void*
stash_get(stash_t* stash, int handle)
{
    assert (stash);
    assert (handle < stash->c && handle >= 0);
    return stash->array[handle];
}

int
stash_size(stash_t* stash)
{
    assert (stash);
    return stash->c;
}
