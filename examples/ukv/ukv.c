/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "hashtable/hashtable.h"
#include "ukv.h"

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

struct ukv {
    struct hashtable* h;
};


/* -----------------------------------------------------------------------------
 * hash and equality functions
 * -------------------------------------------------------------------------- */
unsigned int
hashfn(void* _k) SEI_RONLY;

unsigned int
hashfn(void* _k)
{
    char* k = (char*)_k;
    unsigned long hash = 5381;
    int c;

    while ((c = *k++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


int
eqfn(void* k1, void* k2) SEI_RONLY;

int
eqfn(void* k1, void* k2)
{
    return 0 == strcmp((char*)k1, (char*)k2);
}

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

ukv_t* ukv_init() SEI_SAFE;

ukv_t*
ukv_init()
{
    ukv_t* ukv = (ukv_t*) malloc(sizeof(ukv_t));
    assert (ukv);
    ukv->h = create_hashtable(16, hashfn, eqfn);
    assert (ukv->h);

    return ukv;
}

void
ukv_fini(ukv_t* ukv)
{
    hashtable_destroy(ukv->h, 1);
    free(ukv);
}


/* -----------------------------------------------------------------------------
 * access methods
 * -------------------------------------------------------------------------- */

const char*
ukv_get(ukv_t* ukv, const char* key)
{
    assert (ukv != NULL);
    assert (key != NULL);

    const char* value = hashtable_search(ukv->h, (void*) key);
    return value;
}

const char* ukv_set(ukv_t* ukv, char* key, char* value) SEI_SAFE;

const char*
ukv_set(ukv_t* ukv, char* key, char* value)
{
    assert (ukv != NULL);
    assert (key != NULL);
    assert (value != NULL);

    char* v = hashtable_search(ukv->h, (void*) key);
    if (v) {
        free (key);
        free (value);
        return v;
    }

#ifndef NDEBUG
    int r =
#endif
        hashtable_insert(ukv->h, key, value);

    // assert is removed if NDEBUG is defined
    assert (r != 0);

    return NULL;
}

void
ukv_del(ukv_t* ukv, const char* key)
{
    assert (ukv != NULL);
    assert (key != NULL);

    char* v = hashtable_remove(ukv->h, (void*) key);
    if (v) free(v);
}
