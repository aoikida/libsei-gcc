/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Sergey Arnautov, Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stdarg.h>
#include "fail.h"
#include "debug.h"
#include "config.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */

#include "wts.h"
#include "heap.h"
#ifdef SEI_STACK_INFO
#include "sinfo.h"
#endif

/* N-way DMR redundancy configuration */
#ifndef SEI_DMR_REDUNDANCY
#define SEI_DMR_REDUNDANCY 2
#endif

typedef struct  {
    wts_cb_t func[SEI_DMR_REDUNDANCY];			// function pointers for each phase
    uint32_t anum[SEI_DMR_REDUNDANCY];			// number of arguments for each phase
    uint64_t args[SEI_DMR_REDUNDANCY][WTS_MAX_ARG];	// arguments for each phase

#ifdef SEI_STACK_INFO
    sinfo_t* sinfo[SEI_DMR_REDUNDANCY];
#endif
} wts_item_t;

struct wts {
    int max_items;      	// maximum number of items
    int nitems[SEI_DMR_REDUNDANCY];      	// actual number of items for each phase
    wts_item_t* items; 		// array of items
};

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

wts_t*
wts_init(int max_items)
{
    assert (max_items > 0 && "invalid maximal number of items");
    wts_t* wts = (wts_t*) malloc(sizeof(wts_t));
    assert (wts && "out of memory");
    wts->max_items = max_items;
    wts->items    = (wts_item_t*) malloc(sizeof(wts_item_t)*max_items);
    assert (wts->items && "out of memory");
    bzero(wts->items, sizeof(wts_item_t)*max_items);

    /* Initialize all phase counters to 0 */
    for (int i = 0; i < SEI_DMR_REDUNDANCY; i++) {
        wts->nitems[i] = 0;
    }

    return wts;
}

void
wts_fini(wts_t* wts)
{
    assert (wts);
    free(wts->items);
    free(wts);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

/* Pre-check for wts_flush without executing system calls
 * Returns: 1 if can flush safely, 0 if mismatch detected */
inline int
wts_can_flush(wts_t* wts)
{
    assert(wts);

    /* N-way verification: all phases must have same number of system calls */
    int expected_nitems = wts->nitems[0];
    for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
        if (wts->nitems[p] != expected_nitems)
            return 0;
    }

    wts_item_t* it = &wts->items[0];
    for (int i = 0; i < expected_nitems; ++i, ++it) {
        /* Verify function pointers across all phases */
        wts_cb_t expected_func = it->func[0];
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            if (!it->func[p] || it->func[p] != expected_func)
                return 0;
        }

        /* Verify argument counts across all phases */
        uint32_t expected_anum = it->anum[0];
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            if (it->anum[p] != expected_anum)
                return 0;
        }

        /* Verify argument values across all phases */
        for (int j = 0; j < expected_anum; ++j) {
            uint64_t expected_arg = it->args[0][j];
            for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
                if (it->args[p][j] != expected_arg)
                    return 0;
            }
        }
    }
    return 1;
}

inline void
wts_flush(wts_t* wts)
{
    assert (wts);

    /* N-way verification before executing system calls */
    fail_ifn(wts_can_flush(wts), "N-way system call mismatch");

    wts_item_t* it = &wts->items[0];
    for (int i = 0; i < wts->nitems[0]; ++i, ++it) {
        /* Execute system call using phase 0 data (all phases verified to be identical) */
        it->func[0]((uint64_t*)&it->args[0]);

#ifdef SEI_STACK_INFO
        for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
            if (it->sinfo[p]) {
                sinfo_fini(it->sinfo[p]);
                it->sinfo[p] = NULL;
            }
        }
#endif
    }

    /* Reset all phase counters */
    for (int i = 0; i < SEI_DMR_REDUNDANCY; i++) {
        wts->nitems[i] = 0;
    }
}

void
wts_add(void* w, int p, wts_cb_t fp, int arg_num, ...)
{
	assert(w);
	assert(fp);
	assert ((p >= 0 && p < SEI_DMR_REDUNDANCY) && "invalid p");

	wts_t* wts = (wts_t*) w;
	wts_item_t* it = &wts->items[wts->nitems[p]++];

	assert (wts->nitems[p] + 1 <= wts->max_items && "cannot add item");

	it->func[p] = fp;
	it->anum[p] = arg_num;

	va_list ap;
	va_start (ap, arg_num);

	int i = 0;
	for (i = 0; i < arg_num; i++)
		it->args[p][i] = va_arg (ap, uint64_t);

	va_end (ap);

#ifdef SEI_STACK_INFO
    	assert (it->sinfo[p] == NULL);
    	it->sinfo[p] = sinfo_init(fp);
#endif
}

#ifdef SEI_CPU_ISOLATION
/* ----------------------------------------------------------------------------
 * Rollback: reset waitress queue without executing calls
 * ------------------------------------------------------------------------- */

void
wts_reset(wts_t* wts)
{
    assert(wts);

    /* Clean up stack info for all queued items */
#ifdef SEI_STACK_INFO
    for (int i = 0; i < wts->nitems[0]; i++) {
        wts_item_t* it = &wts->items[i];
        if (it->sinfo[0]) {
            sinfo_fini(it->sinfo[0]);
            it->sinfo[0] = NULL;
        }
        if (it->sinfo[1]) {
            sinfo_fini(it->sinfo[1]);
            it->sinfo[1] = NULL;
        }
    }
#endif

    /* Reset the queue to initial state without executing calls */
    bzero(wts->items, sizeof(wts_item_t) * wts->max_items);
    wts->nitems[0] = 0;
    wts->nitems[1] = 0;
}

#endif /* SEI_CPU_ISOLATION */
