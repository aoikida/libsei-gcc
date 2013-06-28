/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_COW_H_
#define _ASCO_COW_H_
#include <stdint.h>
#include <stdlib.h> // size_t

typedef union {
    struct {
        uint64_t value[1];
    } _uint64_t;
    struct {
        uint32_t value[2];
    } _uint32_t;
    struct {
        uint16_t value[4];
    } _uint16_t;
    struct {
        uint8_t value[8];
    } _uint8_t;
} cow_word_t;

typedef struct cow_entry {
    uintptr_t  wkey;
    cow_word_t wvalue;
    struct cow_entry* next;
} cow_entry_t;

typedef struct cow_buffer {
    cow_entry_t* buffer;
    int size;
    int max_size;
} cow_t;

cow_t* cow_init(int max_size);
void   cow_fini(cow_t* cow);
void   cow_apply(cow_t* cow);
void   cow_show(cow_t* cow);

typedef struct heap heap_t;
void cow_check_apply(heap_t*, cow_t*, heap_t*, cow_t*);

void cow_write_uint8_t (cow_t* cow, uint8_t*  addr, uint8_t  value);
void cow_write_uint16_t(cow_t* cow, uint16_t* addr, uint16_t value);
void cow_write_uint32_t(cow_t* cow, uint32_t* addr, uint32_t value);
void cow_write_uint64_t(cow_t* cow, uint64_t* addr, uint64_t value);

uint8_t  cow_read_uint8_t (cow_t* cow, const uint8_t*  addr);
uint16_t cow_read_uint16_t(cow_t* cow, const uint16_t* oaddr);
uint32_t cow_read_uint32_t(cow_t* cow, const uint32_t* addr);
uint64_t cow_read_uint64_t(cow_t* cow, const uint64_t* addr);

#endif /* _ASCO_COW_H_ */
