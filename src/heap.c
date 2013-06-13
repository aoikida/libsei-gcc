/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

/* -----------------------------------------------------------------------------
 * types, data structures and definitions
 * -------------------------------------------------------------------------- */

#include "heap.h"
#define ALLOC_MAX_SIZE HEAP_100MB //1024

struct allocation {
    size_t size;
    struct allocation* next;
    char data[];
};

/* -----------------------------------------------------------------------------
 * static prototypes
 * -------------------------------------------------------------------------- */

static inline size_t upper_power_of_two(size_t v);
static inline unsigned int_log2(size_t x);

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

heap_t*
heap_init(uint32_t size)
{
    assert (size > ALLOC_MAX_SIZE);

    heap_t* heap = (heap_t*) malloc(sizeof(heap_t) + size);
    bzero(heap, sizeof(heap_t) + size);

    unsigned log2 = int_log2(upper_power_of_two(ALLOC_MAX_SIZE)) + 1;
    heap->free_list = (allocation_t**) malloc(sizeof(allocation_t*)*log2);
    assert (heap->free_list);
    int i;
    for (i = 0; i < log2; ++i) {
        heap->free_list[i] = NULL;
    }
    heap->size = size;
    heap->cursor = 0;

    return heap;
}

void
heap_fini(heap_t* heap)
{
    free(heap->free_list);
    free(heap);
}

/* -----------------------------------------------------------------------------
 * main interface methods
 * -------------------------------------------------------------------------- */

void*
heap_malloc(heap_t* heap, size_t size)
{
    assert (size <= ALLOC_MAX_SIZE);

    size_t pow_2_size = upper_power_of_two(size);
    unsigned log2 = int_log2(pow_2_size);

    // look in free list
    if (heap->free_list[log2]) {
        allocation_t* a = heap->free_list[log2];
        heap->free_list[log2] = a->next;
        a->next = NULL;
        return (void*) a->data;
    }

    // look for a block with enough space
    //memblock_t* block = heap->blocks[0];

    // allocate memory in the block
    //assert (cursor_ + pow_2_size <= block->size && "should not happen");

    allocation_t* a = (allocation_t*) heap->data + heap->cursor;
    a->size = pow_2_size;
    a->next = NULL;
    heap->cursor += pow_2_size + sizeof(allocation_t);
    // for valgrind
    bzero(a->data, size);

    return (void*) a->data;
}

void
heap_free(heap_t* heap, void* ptr)
{
    allocation_t* a = (allocation_t*) (((char*) ptr) - sizeof(allocation_t));
    assert (a->size <= ALLOC_MAX_SIZE);
    unsigned log2 = int_log2(a->size);

    allocation_t* tail = heap->free_list[log2];
    if (tail == NULL) heap->free_list[log2] = a;
    else {
        while (tail->next != NULL) tail = tail->next;
        tail->next = a;
    }
}

inline int
heap_in(heap_t* heap, void* ptr)
{
    return ((char*)ptr >= heap->data
            && (char*) ptr < heap->data + heap->size - sizeof(allocation_t));
}

inline size_t
heap_rel(heap_t* heap, void* ptr)
{
    return ((char*) ptr) - heap->data;
}

inline void*
heap_get(heap_t* heap, size_t rel)
{
    return (void*)(heap->data + rel);
}

/* -----------------------------------------------------------------------------
 * (static) internal methods
 * -------------------------------------------------------------------------- */

// http://stackoverflow.com/questions/466204/rounding-off-to-nearest-power-of-2
size_t
upper_power_of_two(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    // the following if condition expression is a constant expression, the
    // compiler should optimize it away
    if (sizeof(size_t) > 4)
        v |= v >> 32;
    return v + 1;
}

// obvious implementation from
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
unsigned
int_log2(size_t x)
{
    unsigned r = 0;
    while (x >>= 1) ++r;
    return r;
}
