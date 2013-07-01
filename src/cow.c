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
 * helper macros
 * -------------------------------------------------------------------------- */

#define GETWKEY(addr)        (((uintptr_t) addr) >> 3)
#define GETWADDR(wkey)       (((uintptr_t) wkey) << 3)

#define TYPEMASK(addr, type) ( (uintptr_t) addr & (sizeof(type) - 1))
#define PICKMASK(addr, type) (((uintptr_t) addr & 0x07) >> (sizeof(type) >> 1))

#define WKEY(e) (e->wkey)
#define WVAL(e, type, idx) (e->wvalue._##type.value[idx])

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
cow_apply_cmp(cow_t* cow1, cow_t* cow2)
{
    assert (cow1->size == cow2->size && "cow buffers differ (size)");
    int i;
    for (i = 0; i < cow1->size; ++i) {
        cow_entry_t* e1 = &cow1->buffer[i];
        cow_entry_t* e2 = &cow2->buffer[i];

        assert (WKEY(e1) != WKEY(e2) && "cow entries point to same address");

        uint64_t v1 = WVAL(e1, uint64_t, 0);
        uint64_t v2 = WVAL(e2, uint64_t, 0);
        assert (v1 == v2 && "cow entries differ (value)");
        *(uint64_t*) GETWADDR(e1->wkey) = v1;
    }
    cow1->size = 0;
    cow2->size = 0;
}

void
cow_apply_heap(heap_t* heap1, cow_t* cow1, heap_t* heap2, cow_t* cow2)
{
    assert (cow1->size == cow2->size && "cow buffers differ (size)");
    int i;
    for (i = 0; i < cow1->size; ++i) {
        cow_entry_t* e1 = &cow1->buffer[i];
        cow_entry_t* e2 = &cow2->buffer[i];

        assert (WKEY(e1) != WKEY(e2) && "cow entries point to same address");

        uint64_t v1 = WVAL(e1, uint64_t, 0);
        uint64_t v2 = WVAL(e2, uint64_t, 0);
        assert ((v1 == v2
                 || heap_rel(heap1, (void*) v1) == heap_rel(heap2, (void*) v2))
                && "cow entries differ (value)");
        *(uint64_t*) GETWADDR(e1->wkey) = v1;
        *(uint64_t*) GETWADDR(e2->wkey) = v2;
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
        uintptr_t addr = GETWADDR(e->wkey);
        *((uint64_t*) addr) = WVAL(e, uint64_t, 0);
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
cow_find(cow_t* cow, uintptr_t wkey)
{
    int i;
    cow_entry_t* e = cow->buffer;

    for (i = cow->size; i > 0; --i, ++e)
        if (e->wkey == wkey) return e;

    return NULL;
}

/* -----------------------------------------------------------------------------
 * reading and writing
 * -------------------------------------------------------------------------- */

#define COW_READ(type) inline                                           \
    type cow_read_##type(cow_t* cow, const type* addr)                  \
    {                                                                   \
        cow_entry_t* e = cow_find(cow, GETWKEY(addr));                  \
        if (e == NULL) {                                                \
            return *addr;                                               \
        }                                                               \
        if (TYPEMASK(addr, type) == 0) {                                \
            return WVAL(e, type, PICKMASK(addr,type));                  \
        } else {                                                        \
            assert (0 && "cant handle unaligned accesses");             \
        }                                                               \
    }
COW_READ(uint8_t)
COW_READ(uint16_t)
COW_READ(uint32_t)
COW_READ(uint64_t)


#define COW_WRITE(type) inline                                          \
    void cow_write_##type(cow_t* cow, type* addr, type value)           \
    {                                                                   \
        if (TYPEMASK(addr, type) != 0) {                                \
            assert (0 && "cant handle unaligned accesses");             \
        }                                                               \
        cow_entry_t* e = cow_find(cow, GETWKEY(addr));                  \
        if (!e) {                                                       \
            if (cow->size == cow->max_size) cow_realloc(cow);           \
            e = &cow->buffer[cow->size++];                              \
            assert (e);                                                 \
            WKEY(e) = GETWKEY(addr);                                    \
            WVAL(e, uint64_t, 0) = *(uint64_t*) GETWADDR(e->wkey);      \
            DLOG3("[%s:%d] creating new entry x%x = x%x  (x%x)\n",      \
                       __FILE__, __LINE__, WKEY(e),                     \
                   WVAL(e, uint64_t, 0), value);                        \
        }                                                               \
        WVAL(e, type, PICKMASK(addr,type)) = value;                     \
        e->next = e+1;                                                  \
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
    if (cow->size > 100) DLOG1("cow size: %d\n", cow->size);
    int i;
    DLOG3("----------\n");
    DLOG3("COW BUFFER %p:\n", cow);
    for (i = 0; i < cow->size; ++i) {
        DLOG3("addr = %16p value = %ld x%lx \n",
              cow->buffer[i].wkey,
              cow->buffer[i].wvalue._uint64_t.value[0],
              cow->buffer[i].wvalue._uint64_t.value[0]);
    }
    DLOG3("----------\n");
}
