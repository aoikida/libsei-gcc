/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_TALLOC_H_
#define _SEI_TALLOC_H_

#include <stddef.h>
#include "heap.h"

typedef struct talloc talloc_t;

talloc_t* talloc_init(heap_t* heap);
void      talloc_fini(talloc_t* talloc);
void*     talloc_malloc(talloc_t* talloc, size_t size);
void      talloc_switch(talloc_t* talloc);
void      talloc_clean(talloc_t* talloc);

#endif /* _SEI_TALLOC_H_ */
