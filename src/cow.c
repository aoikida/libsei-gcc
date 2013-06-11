/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <stdio.h>
#include <assert.h>
#include "cow.h"
#include "heap.h"
#include "debug.h"

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

cow_t*
cow_init(int max_size)
{
    cow_t* cow = (cow_t*) malloc(sizeof(cow_t));
    assert(cow);

    cow->max_size = max_size;
    cow->size = 0;
    cow->buffer = (cow_entry_t*) malloc(max_size*sizeof(cow_entry_t));
    assert (cow->buffer);

    return cow;
}

void
cow_fini(cow_t* cow)
{
    free(cow->buffer);
    free(cow);
}

/* -----------------------------------------------------------------------------
 * main interface methods
 * -------------------------------------------------------------------------- */

void
cow_check_apply(heap_t* heap1, cow_t* cow1, heap_t* heap2, cow_t* cow2)
{
    assert (cow1->size == cow2->size && "cow buffers differ (size)");
    int i;
    for (i = 0; i < cow1->size; ++i) {
        cow_entry_t* e1 = &cow1->buffer[i];
        cow_entry_t* e2 = &cow2->buffer[i];

        assert (e1->size == e2->size && "cow entries differ (size)");
        assert (e1->addr != e2->addr && "cow entries point to same address");

        switch(e1->size) {
        case sizeof(uint8_t):
            assert (e1->value_uint8_t == e2->value_uint8_t
                    && "cow entries differ (value)");
            *((uint8_t*) e1->addr) = e1->value_uint8_t;
            *((uint8_t*) e2->addr) = e2->value_uint8_t;
            break;
        case sizeof(uint16_t):
            assert (e1->value_uint16_t == e2->value_uint16_t
                    && "cow entries differ (value)");
            *((uint16_t*) e1->addr) = e1->value_uint16_t;
            *((uint16_t*) e2->addr) = e2->value_uint16_t;
            break;
        case sizeof(uint32_t):
            assert (e1->value_uint32_t == e2->value_uint32_t
                    && "cow entries differ (value)");
            *((uint32_t*) e1->addr) = e1->value_uint32_t;
            *((uint32_t*) e2->addr) = e2->value_uint32_t;
            break;
        case sizeof(uint64_t):
            assert ((e1->value_uint64_t == e2->value_uint64_t
                     || heap_rel(heap1, e1->addr) == heap_rel(heap2, e2->addr))
                    && "cow entries differ (value)");
            *((uint64_t*) e1->addr) = e1->value_uint64_t;
            *((uint64_t*) e2->addr) = e2->value_uint64_t;
            break;
        default:
            assert (0 && "invalid size");
        }
    }
    cow1->size = 0;
    cow2->size = 0;
}

void
cow_apply(cow_t* cow)
{
    int i;
    for (i = 0; i < cow->size; ++i) {
        cow_entry_t* e = &cow->buffer[i];
        switch(e->size) {
        case sizeof(uint8_t):
            *((uint8_t*) e->addr) = e->value_uint8_t;
            break;
        case sizeof(uint16_t):
            *((uint16_t*) e->addr) = e->value_uint16_t;
            break;
        case sizeof(uint32_t):
            *((uint32_t*) e->addr) = e->value_uint32_t;
            break;
        case sizeof(uint64_t):
            *((uint64_t*) e->addr) = e->value_uint64_t;
            break;
        default:
            assert (0 && "invalid size");
        }
    }
    cow->size = 0;
}

static inline void
cow_realloc(cow_t* cow)
{
    assert (0);
    cow->max_size *= 2;
    cow->buffer = (cow_entry_t*) realloc(cow->buffer,
                                         cow->max_size*sizeof(cow_entry_t));
    assert (cow->buffer);
}


cow_entry_t*
cow_find(cow_t* cow, void* addr)
{
    int i;
    cow_entry_t* e = cow->buffer;

    for (i = cow->size; i > 0; --i, ++e)
        if (e->addr == addr) return e;

    return NULL;
}

/* -----------------------------------------------------------------------------
 * reading and writing
 * -------------------------------------------------------------------------- */

#define COW_READ(type) inline                                           \
    type cow_read_##type(cow_t* cow, const type* addr)                  \
    {                                                                   \
        cow_entry_t* e = cow_find(cow, (void*)addr);                    \
        if (e) {                                                        \
            return e->value_##type;                                     \
        } else {                                                        \
            return *addr;                                               \
        }                                                               \
    }
COW_READ(uint8_t)
COW_READ(uint16_t)
COW_READ(uint32_t)
COW_READ(uint64_t)


#define COW_WRITE(type) inline                                          \
    void cow_write_##type(cow_t* cow, type* addr, type value)           \
    {                                                                   \
        cow_entry_t* e = cow_find(cow, (void*) addr);                   \
        if (!e) {                                                       \
            if (cow->size == cow->max_size) cow_realloc(cow);           \
            e = &cow->buffer[cow->size++];                              \
            assert (e);                                                 \
        }                                                               \
        e->addr = (void*) addr;                                         \
        e->value_##type = value;                                        \
            e->size = sizeof(type);                                     \
    }
COW_WRITE(uint8_t)
COW_WRITE(uint16_t)
COW_WRITE(uint32_t)
COW_WRITE(uint64_t)


/* -----------------------------------------------------------------------------
 * helper methods
 * -------------------------------------------------------------------------- */

void
cow_show(cow_t* cow)
{
    if (cow->size > 100) printf("cow size: %d\n", cow->size);
    int i;
    DLOG3("----------\n");
    DLOG3("COW BUFFER %p:\n", cow);
    for (i = 0; i < cow->size; ++i) {
        DLOG3("addr = %16p value = %ld \n",
              cow->buffer[i].addr,
              cow->buffer[i].value_uint8_t);
    }
    DLOG3("----------\n");
}
