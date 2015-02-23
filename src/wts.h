/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Sergey Arnautov, Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#ifndef _SEI_WAITRESS_H_
#define _SEI_WAITRESS_H_

#include <stdint.h>
#include "tmi_sc.h"


typedef struct wts wts_t;
typedef int (*wts_cb_t)(uint64_t* args);

wts_t* 	wts_init(int max_items);
void	wts_fini(wts_t* wts);
void	wts_add(void* w, int p, wts_cb_t fp, int arg_num, ...);
void	wts_flush(wts_t* wts);


#endif /* _SEI_WAITRESS_H_ */
