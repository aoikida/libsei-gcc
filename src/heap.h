/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_HEAP_H_
#define _ASCO_HEAP_H_
#include <stdint.h>
#include <stdlib.h>

typedef struct allocation allocation_t;
typedef struct heap {
    uint64_t size;
    uint64_t cursor;
    allocation_t** free_list;
    char data[];
} heap_t;


heap_t* heap_init(uint32_t size);
void*   heap_malloc(heap_t* heap, size_t size);
void    heap_free(heap_t* heap, void* ptr);
void    heap_fini(heap_t* heap);
int     heap_in(heap_t* heap, void* ptr);
size_t  heap_rel(const heap_t* heap, const void* ptr);
void*   heap_get(heap_t* heap, size_t rel);

#define HEAP_1MB   1024*1024
#define HEAP_10MB  10*HEAP_1MB
#define HEAP_50MB  50*HEAP_1MB
#define HEAP_100MB 100*HEAP_1MB
#define HEAP_500MB 500*HEAP_1MB
#define HEAP_1GB   1000*HEAP_1MB

#ifndef COW_SIZE
#define COW_SIZE 100000
#endif

#endif /* _ASCO_HEAP_H_ */
