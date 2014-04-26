/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _ASCO_STASH_H_
#define _ASCO_STASH_H_
#include <stdint.h>
#include <stdlib.h>

typedef struct stash stash_t;
stash_t* stash_init();
void     stash_fini(stash_t* stash);

int      stash_size(stash_t* stash);
int      stash_add(stash_t* stash, void* item);
void*    stash_get(stash_t* stash, int handle);

#endif /* _ASCO_STASH_H_ */
