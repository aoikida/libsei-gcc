/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2015 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_H_
#define _SEI_H_
#include <stddef.h>
#include <stdint.h>

typedef struct sei sei_t;

sei_t*   sei_init();
void     sei_fini(sei_t* sei);
void     sei_begin(sei_t* sei);
void     sei_switch(sei_t* sei);
void     sei_commit(sei_t* sei);
void*    sei_malloc(sei_t* sei, size_t size);
void*    sei_calloc(sei_t* sei, size_t nmemb, size_t size);
void     sei_free(sei_t* sei, void* ptr);
int      sei_getp(sei_t* sei);
void     sei_setp(sei_t* sei, int p);
int      sei_shift(sei_t* sei, int handle);

/* wts functions */
void*    sei_get_wts(sei_t* sei);


/* heap-mode functions */
void*    sei_malloc2(sei_t* sei, size_t size);
void     sei_free2(sei_t* sei, void* ptr1, void* ptr2);
void*    sei_other(sei_t* sei, void* addr);

/* prepare and output functions */
int      sei_prepare(sei_t* sei, const void* ptr, size_t size, uint32_t crc,
                     int ro);
void     sei_prepare_nm(sei_t* sei);
void     sei_output_append(sei_t* sei, const void* ptr, size_t size);
void     sei_output_done(sei_t* sei);
uint32_t sei_output_next(sei_t* sei);

/* memory interface */
uint8_t  sei_read_uint8_t (sei_t* sei, const uint8_t*  addr);
uint16_t sei_read_uint16_t(sei_t* sei, const uint16_t* addr);
uint32_t sei_read_uint32_t(sei_t* sei, const uint32_t* addr);
uint64_t sei_read_uint64_t(sei_t* sei, const uint64_t* addr);

void sei_write_uint8_t (sei_t* sei, uint8_t*  addr, uint8_t  value);
void sei_write_uint16_t(sei_t* sei, uint16_t* addr, uint16_t value);
void sei_write_uint32_t(sei_t* sei, uint32_t* addr, uint32_t value);
void sei_write_uint64_t(sei_t* sei, uint64_t* addr, uint64_t value);

#ifdef SEI_CPU_ISOLATION
/* Rollback and non-destructive commit for SDC recovery */
void     sei_rollback(sei_t* sei);
int      sei_try_commit(sei_t* sei);
#endif

#endif /* _SEI_H_ */
