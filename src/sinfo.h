/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_SINFO_H_
#define _ASCO_SINFO_H_

#define MAX_DEPTH 24
#define MAX_ALLOC 100000

typedef struct sinfo sinfo_t;

sinfo_t* sinfo_init(void* addr);
void     sinfo_fini(sinfo_t* sinfo);
void     sinfo_update(sinfo_t* sinfo, void* addr);
void     sinfo_show(sinfo_t*);

#endif /* _ASCO_SINFO_H_ */
