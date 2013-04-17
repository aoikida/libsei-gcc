/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_H_
#define _ASCO_H_
#include <stdlib.h>
#include <stdint.h>

typedef struct asco asco_t;

asco_t* asco_init();
void    asco_fini(asco_t* asco);
void    asco_begin(asco_t* asco);
void    asco_switch(asco_t* asco);
void    asco_commit(asco_t* asco);
void*   asco_malloc(asco_t* asco, size_t size);
void*   asco_calloc(asco_t* asco, size_t nmemb, size_t size);
void    asco_free(asco_t* asco, void* ptr);

void    asco_free2(asco_t* asco, void* ptr1, void* ptr2);

uint8_t  asco_read_uint8_t (asco_t* asco, const uint8_t*  addr);
uint16_t asco_read_uint16_t(asco_t* asco, const uint16_t* addr);
uint32_t asco_read_uint32_t(asco_t* asco, const uint32_t* addr);
uint64_t asco_read_uint64_t(asco_t* asco, const uint64_t* addr);

void asco_write_uint8_t (asco_t* asco, uint8_t*  addr, uint8_t  value);
void asco_write_uint16_t(asco_t* asco, uint16_t* addr, uint16_t value);
void asco_write_uint32_t(asco_t* asco, uint32_t* addr, uint32_t value);
void asco_write_uint64_t(asco_t* asco, uint64_t* addr, uint64_t value);

#endif /* _ASCO_H_ */
