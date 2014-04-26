/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _ASCO_IBUF_H_
#define _ASCO_IBUF_H_
#include <stdint.h>
#include <stdlib.h>
#include "crc.h"

typedef enum {READ_ONLY, READ_WRITE} ibuf_mode_t;
typedef struct ibuf ibuf_t;
ibuf_t* ibuf_init();
void    ibuf_fini(ibuf_t* ibuf);
int     ibuf_prepare(ibuf_t* ibuf, const void* ptr, size_t size, uint32_t crc,
                    ibuf_mode_t mode);
int     ibuf_correct(ibuf_t* ibuf);
void    ibuf_switch(ibuf_t* ibuf);

#endif /* _ASCO_IBUF_H_ */
