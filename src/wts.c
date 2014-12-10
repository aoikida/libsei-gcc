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
#ifdef ASCO_STACK_INFO
#include "sinfo.h"
#endif

typedef struct  {
    wts_cb_t func[2];			// function
    uint32_t anum[2];			// number of arguments
    uint64_t args[2][WTS_MAX_ARG];	// arguments for the call, also stack args

#ifdef ASCO_STACK_INFO
    sinfo_t* sinfo[2];
#endif
} wts_item_t;

struct wts {
    int max_items;      	// maximum number of items
    int nitems[2];      	// actual number of items
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
    wts->nitems[0] = wts->nitems[1] = 0;

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

inline void
wts_flush(wts_t* wts)
{
    assert (wts);
    fail_ifn(wts->nitems[0] == wts->nitems[1], "number of items differ");

    wts_item_t* it = &wts->items[0];
    int i = 0;
    for (; i < wts->nitems[0]; ++i, ++it) {
        fail_ifn(it->func[0] && it->func[1], "only one pointer passed");
        fail_ifn(it->func[0] == it->func[1], "pointers differ");
        fail_ifn(it->anum[0] == it->anum[1], "number of args differ");

        int j = 0;
//        for (; j < it->anum[0]; ++j)
//        	fail_ifn(it->args[0][j] == it->args[1][j], "args differ");

        it->func[0]((uint64_t*)&it->args[0]);

#ifdef ASCO_STACK_INFO
        sinfo_fini(it->sinfo[0]);
        sinfo_fini(it->sinfo[1]);
        it->sinfo[0] = it->sinfo[1] = NULL;
#endif
    }
    wts->nitems[0] = wts->nitems[1] = 0;
}

void
wts_add(void* w, int p, wts_cb_t fp, int arg_num, ...)
{
	assert(w);
	assert(fp);
	assert ((p == 0 || p == 1) && "invalid p");

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

#ifdef ASCO_STACK_INFO
    	assert (it->sinfo[p] == NULL);
    	it->sinfo[p] = sinfo_init(fp);
#endif
}
