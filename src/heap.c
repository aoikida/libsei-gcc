/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>   // sysconf
#include <errno.h>    // perror

/* -----------------------------------------------------------------------------
 * types, data structures and definitions
 * -------------------------------------------------------------------------- */

#include "heap.h"
#ifndef ALLOC_MAX_SIZE
#define ALLOC_MAX_SIZE HEAP_1MB //1024
#endif

struct allocation {
    uint64_t size;
    struct allocation* next;
    uint64_t data[];
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
    heap_t* heap = NULL;

    if (size > 0) {
        // preallocated heap
        assert (size > ALLOC_MAX_SIZE);

        long page_size = sysconf(_SC_PAGESIZE);

        int r = posix_memalign((void**) &heap, page_size, sizeof(heap_t) + size);
        if (r == EINVAL) {
            fprintf(stderr, "alignment argument was not a power of two\n");
            exit (EXIT_FAILURE);
        }
        if (r == ENOMEM) {
            fprintf(stderr, "out of memory\n");
            exit (EXIT_FAILURE);
        }
        bzero(heap, sizeof(heap_t) + size);
    } else {
        // no preallocation, use malloc and free directly
        heap = malloc(sizeof(heap_t));
        assert (heap && "out of memory");
    }

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

    size_t tsize = size + sizeof(allocation_t);
    size_t pow_2_size = upper_power_of_two(tsize);
    unsigned log2 = int_log2(pow_2_size);

    // look in free list
    if (heap->free_list[log2]) {
        allocation_t* a = heap->free_list[log2];
        heap->free_list[log2] = a->next;
        a->next = NULL;
        return (void*) a->data;
    }

    allocation_t* a = NULL;

    if (heap->size > 0) {
        // allocate memory in the block
        assert (heap->cursor + pow_2_size < heap->size
                && "out of memory");
        if (heap->cursor + pow_2_size >= heap->size)
            return NULL;

        a = (allocation_t*) ((char*)heap->data + heap->cursor);
        heap->cursor += pow_2_size;
        // for valgrind
        bzero(a->data, size);
    } else {
        //int r = posix_memalign((void**) &a, sizeof(uint64_t), tsize);
        //assert (r == 0 && "allocation error");
        a = (allocation_t*) malloc(tsize);
        assert (a && "out of memory");
    }

    a->size = pow_2_size;
    a->next = NULL;
    return (void*) a->data;
}

void
heap_free(heap_t* heap, void* ptr)
{
    if (heap->size > 0)
        assert (heap_in(heap, ptr) && "freeing data not in heap");

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
    if (heap->size == 0) return 1;

    return ((char*)ptr >= heap->data
            && (char*) ptr < heap->data + heap->size);
}

inline size_t
heap_rel(const heap_t* heap, const void* ptr)
{
    assert (heap->size > 0 && "no heap preallocation");
    return ((char*) ptr) - heap->data;
}

inline void*
heap_get(heap_t* heap, size_t rel)
{
    assert (heap->size > 0 && "no heap preallocation");
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
