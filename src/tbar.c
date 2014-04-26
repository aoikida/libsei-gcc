/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "tbar.h"

#ifdef TBAR_PTHREAD
#include <pthread.h>
#endif

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */

typedef struct {
    uint64_t count;
    uint64_t mark;
} tbar_item_t;

struct tbar {
    int me; /* thread identifier (in global tbar the total # of threads) */
    int threads;
    int max_threads;
    tbar_item_t* items;
    struct tbar* global;
#ifdef TBAR_PTHREAD
    pthread_mutex_t lock;
#endif
};


/* ----------------------------------------------------------------------------
 * internal methods
 * ------------------------------------------------------------------------- */

#ifdef TBAR_PTHREAD
static void
tbar_lock(tbar_t* tbar)
{
    assert (tbar);
    pthread_mutex_lock(&tbar->lock);
}

static void
tbar_unlock(tbar_t* tbar)
{
    assert (tbar);
    pthread_mutex_unlock(&tbar->lock);
}
#endif

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

tbar_t*
tbar_init(int max_threads, tbar_t* global)
{
    tbar_t* tbar = (tbar_t*) malloc(sizeof(tbar_t));
    assert (tbar);
    tbar->threads = 0;
    tbar->max_threads = max_threads;

    tbar->items = (tbar_item_t*) malloc(sizeof(tbar_item_t)*max_threads);
    assert (tbar->items);
    bzero(tbar->items, sizeof(tbar_item_t)*max_threads);

#ifdef TBAR_PTHREAD
    if (global != NULL) {
        tbar_lock(global);
        tbar->me = global->me++;
        tbar_unlock(global);
    } else {
        tbar->me = 0;
        pthread_mutex_init(&tbar->lock, NULL);
    }
#else /* TBAR_PTHREAD */
    if (global != NULL) {
        tbar->me = __sync_fetch_and_add(&global->me, 1);
    } else {
        tbar->me = 0;
    }
#endif /* TBAR_PTHREAD */
    tbar->global = global;

    return tbar;
}

tbar_t*
tbar_idup(tbar_t* tbar_orig)
{
    assert (tbar_orig);
    assert (tbar_orig->global && "should not be called with global tbar");
    tbar_t* tbar = (tbar_t*) malloc(sizeof(tbar_t));
    assert (tbar);
    memcpy(tbar, tbar_orig, sizeof(tbar_t));

    tbar->items = (tbar_item_t*) malloc(sizeof(tbar_item_t)*tbar->max_threads);
    assert (tbar->items);
    memcpy(tbar->items, tbar_orig->items, sizeof(tbar_item_t)*tbar->max_threads);
    return tbar;
}

void
tbar_fini(tbar_t* tbar)
{
    assert (tbar);
    assert (tbar->items);
#ifdef TBAR_PTHREAD
    if (!tbar->global) {
        pthread_mutex_destroy(&tbar->lock);
    }
#endif
    free(tbar->items);
    free(tbar);
}


/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

void
tbar_enter(tbar_t* tbar)
{
    assert (tbar);
    assert (tbar->global);
    tbar_t* global = tbar->global;

#ifdef TBAR_PTHREAD
    tbar_lock(global);
    global->items[tbar->me].count++;

    assert (global->items[tbar->me].count % 2 == 1);
    tbar_unlock(global);
#else /* TBAR_PTHREAD */
#ifndef NDEBUG
    uint64_t c =
#endif
        //__sync_fetch_and_add(&global->items[tbar->me].count, 1);
        __atomic_fetch_add(&global->items[tbar->me].count, 1, __ATOMIC_RELEASE);
        //global->items[tbar->me].count++;

    assert (c % 2 == 0);
#endif /* TBAR_PTHREAD */
}

void
tbar_leave(tbar_t* tbar)
{
    assert (tbar);
    assert (tbar->global);
    tbar_t* global = tbar->global;

#ifdef TBAR_PTHREAD
    tbar_lock(global);
    // update my entry
    global->items[tbar->me].count++;
    assert (global->items[tbar->me].count % 2 == 0);
    // reset marks while memcpy since global always has marks set to 0
    // this memcpy has to read fresh values!
    memcpy(tbar->items, global->items, sizeof(tbar_item_t)*tbar->global->me);
    tbar_unlock(global);
#else /* TBAR_PTHREAD */
#ifndef NDEBUG
    uint64_t c =
#endif
        __atomic_fetch_add(&global->items[tbar->me].count, 1, __ATOMIC_RELEASE);
        //global->items[tbar->me].count++;
    assert (c % 2 == 1);
    int i;
    int m = global->me; // I'm feeling lucky
    for (i = 0; i < m; ++i) {
        // atomic read item's count
        tbar->items[i].count =
            __atomic_load_n(&global->items[i].count, __ATOMIC_ACQUIRE);
            //global->items[i].count;
        // reset mark
        tbar->items[i].mark = 0;
    }
#endif /* TBAR_PTHREAD */
}

int
tbar_check(tbar_t* tbar)
{
    assert (tbar);
    int i;
    int r = 1;

    for (i = 0; i < tbar->global->me; ++i) {

        // if already marked, skip
        if (tbar->items[i].mark)
            continue;

        // if ok, mark and skip
        if (tbar->items[i].count % 2 == 0) {
            tbar->items[i].mark = 1;
            continue;
        }

        // else check for updates
        // don't have to synchronize this check
        int count = tbar->global->items[i].count;
        if (count > tbar->items[i].count) {
            tbar->items[i].mark = 1;
            continue;
        }

        // if not ok, set r = 0 but don't break. Since already here,
        // check remainder threads
        r = 0;
    }
    //r = 1;
    return r;
}
