/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_ABUF_H_
#define _SEI_ABUF_H_
#include <stdint.h>
#include <stdlib.h>

typedef struct abuf abuf_t;
abuf_t* abuf_init(int max_size);
void    abuf_fini(abuf_t* abuf);
int     abuf_size(abuf_t* abuf);
void    abuf_clean(abuf_t* abuf);
void    abuf_rewind(abuf_t* abuf);
void    abuf_cmp(abuf_t* a1, abuf_t* a2);

void    abuf_swap(abuf_t* abuf);

#if SEI_DMR_REDUNDANCY == 2
void    abuf_cmp_heap(abuf_t* a1, abuf_t* a2);
#endif

void    abuf_check_duplicates(abuf_t* a1);
void    abuf_push(abuf_t* abuf, void* addr, uint64_t value);
void*   abuf_pop (abuf_t* abuf, uint64_t* value);


void abuf_push_uint8_t (abuf_t* abuf, uint8_t*  addr, uint8_t  value);
void abuf_push_uint16_t(abuf_t* abuf, uint16_t* addr, uint16_t value);
void abuf_push_uint32_t(abuf_t* abuf, uint32_t* addr, uint32_t value);
void abuf_push_uint64_t(abuf_t* abuf, uint64_t* addr, uint64_t value);

uint8_t  abuf_pop_uint8_t (abuf_t* abuf, const uint8_t*  addr);
uint16_t abuf_pop_uint16_t(abuf_t* abuf, const uint16_t* addr);
uint32_t abuf_pop_uint32_t(abuf_t* abuf, const uint32_t* addr);
uint64_t abuf_pop_uint64_t(abuf_t* abuf, const uint64_t* addr);

#ifdef SEI_CPU_ISOLATION
void    abuf_restore(abuf_t* abuf);
int     abuf_try_cmp(abuf_t* a1, abuf_t* a2);

#if SEI_DMR_REDUNDANCY == 2
int     abuf_try_cmp_heap(abuf_t* a1, abuf_t* a2);
#endif

#if SEI_DMR_REDUNDANCY >= 3
int     abuf_try_cmp_heap_nway(abuf_t** buffers, int n);
#endif
#endif

/* N-way専用関数 (N>=3でのみ利用可能) */
#if SEI_DMR_REDUNDANCY >= 3
void    abuf_cmp_heap_nway(abuf_t** buffers, int n);
#endif

#endif /* _SEI_ABUF_H_ */
