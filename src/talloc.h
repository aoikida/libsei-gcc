/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_TALLOC_H_
#define _ASCO_TALLOC_H_

typedef struct talloc talloc_t;

talloc_t* talloc_init();
void      talloc_fini(talloc_t* talloc);
void*     talloc_malloc(talloc_t* talloc, size_t size);
void      talloc_switch(talloc_t* talloc);
void      talloc_clean(talloc_t* talloc);

#endif /* _ASCO_TALLOC_H_ */
