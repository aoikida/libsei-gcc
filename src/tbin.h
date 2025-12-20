/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_TBIN_H_
#define _SEI_TBIN_H_

#include <stdint.h>
#include "heap.h"

typedef struct tbin tbin_t;

tbin_t* tbin_init(int max_items, heap_t* heap);
void    tbin_fini(tbin_t* tbin);
void    tbin_add(tbin_t* tbin, void* ptr, int p);
int     tbin_can_flush(tbin_t* tbin);
void    tbin_flush(tbin_t* tbin);
void    tbin_reset(tbin_t* tbin);

#endif /* _SEI_TBIN_H_ */
