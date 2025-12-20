/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_OBUF_H_
#define _SEI_OBUF_H_
#include <stdint.h>
#include <stdlib.h>

typedef struct obuf obuf_t;
obuf_t* obuf_init(int max_msgs);
void    obuf_fini(obuf_t* obuf);
int     obuf_size(obuf_t* obuf);

void     obuf_push(obuf_t* obuf, const void* ptr, size_t size);
void     obuf_done(obuf_t* obuf);
void     obuf_close(obuf_t* obuf);
uint32_t obuf_pop(obuf_t* obuf);
void     obuf_reset(obuf_t* obuf);

#endif /* _SEI_OBUF_H_ */
