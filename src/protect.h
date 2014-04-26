/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _PROTECT_H_
#define _PROTECT_H_

#include <signal.h>
#include <stddef.h>
typedef enum { READ, WRITE } protect_t;

void  protect_mem(void* addr, size_t size, protect_t protection);
void  protect_setsignal();

#endif /* _PROTECT_H_ */
