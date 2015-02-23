/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_TBAR_H_
#define _SEI_TBAR_H_
#include <stdint.h>

typedef struct tbar tbar_t;
tbar_t* tbar_init(int max_threads, tbar_t* global);
tbar_t* tbar_idup(tbar_t* tbar_orig);
void    tbar_fini(tbar_t* tbar);

void    tbar_enter(tbar_t* tbar);
void    tbar_leave(tbar_t* tbar);
int     tbar_check(tbar_t* tbar);

#endif /* _SEI_TBAR_H_ */
