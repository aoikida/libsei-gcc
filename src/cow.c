/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <stdio.h>
#include <assert.h>

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

#include "cow.h"
#include "heap.h"
#include "debug.h"
#ifdef ASCO_STACK_INFO
#include "sinfo.h"
#endif

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
#ifdef ASCO_STACK_INFO
    sinfo_t* sinfo;
#endif
} cow_entry_t;

struct cow_buffer {
    cow_entry_t* buffer;
    int size;
    int max_size;

#ifdef COW_STATS
    struct {
        uint64_t miss;
        uint64_t size;
        uint64_t iter;
        uint64_t lkup;
        uint64_t count;
    } stats;

    struct {
        uint64_t miss;
        uint64_t iter;
        uint64_t lkup;
    } stats_tr;
#endif
};

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

#ifdef COW_STATS
    cow->stats.size = 0;
    cow->stats.iter = 0;
    cow->stats.miss = 0;
    cow->stats.lkup = 0;
    cow->stats.count = 0;
    cow->stats_tr.miss = 0;
    cow->stats_tr.iter = 0;
    cow->stats_tr.lkup = 0;
#endif

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

        //assert (WKEY(e1) != WKEY(e2) && "entries point to same addresses");
        assert (WKEY(e1) == WKEY(e2) && "entries point to different addresses");

        uint64_t v1 = WVAL(e1, uint64_t, 0);
        uint64_t v2 = WVAL(e2, uint64_t, 0);
        assert (v1 == v2 && "cow entries differ (value)");
        *(uint64_t*) GETWADDR(e1->wkey) = v1;

#ifdef ASCO_STACK_INFO
        if (e1->sinfo) {
            sinfo_fini(e1->sinfo);
            e1->sinfo = NULL;
        }
        if (e2->sinfo) {
            sinfo_fini(e2->sinfo);
            e2->sinfo = NULL;
        }
#endif
    }

#ifdef COW_STATS
    // stats
    cow1->stats.size   += cow1->size;
    cow1->stats.miss   += cow1->stats_tr.miss;
    cow1->stats.iter   += cow1->stats_tr.iter;
    cow1->stats.lkup   += cow1->stats_tr.lkup;
    cow1->stats_tr.miss = 0;
    cow1->stats_tr.iter = 0;
    cow1->stats_tr.lkup = 0;
    ++cow1->stats.count;

    if (cow1->stats.count % 1000 == 0) {
        printf("mean cow size = %f\n", cow1->stats.size*1.0/cow1->stats.count);
        printf("mean cow miss = %f\n", cow1->stats.miss*1.0/cow1->stats.count);
        printf("mean cow iter = %f\n", cow1->stats.iter*1.0/cow1->stats.count);
        printf("mean cow lkup = %f\n", cow1->stats.lkup*1.0/cow1->stats.count);
    }
#endif

    // cleanup
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

#ifdef ASCO_STACK_INFO
        if (e1->sinfo) {
            sinfo_fini(e1->sinfo);
            e1->sinfo = NULL;
        }
        if (e2->sinfo) {
            sinfo_fini(e2->sinfo);
            e2->sinfo = NULL;
        }
#endif
    }

#ifdef COW_STATS
    // stats
    cow1->stats.size   += cow1->size;
    cow1->stats.miss   += cow1->stats_tr.miss;
    cow1->stats.iter   += cow1->stats_tr.iter;
    cow1->stats.lkup   += cow1->stats_tr.lkup;
    cow1->stats_tr.miss = 0;
    cow1->stats_tr.iter = 0;
    cow1->stats_tr.lkup = 0;
    ++cow1->stats.count;

    if (cow1->stats.count % 1000 == 0) {
        printf("mean cow size = %f\n", cow1->stats.size*1.0/cow1->stats.count);
        printf("mean cow miss = %f\n", cow1->stats.miss*1.0/cow1->stats.count);
        printf("mean cow iter = %f\n", cow1->stats.iter*1.0/cow1->stats.count);
        printf("mean cow lkup = %f\n", cow1->stats.lkup*1.0/cow1->stats.count);
    }
#endif

    // cleaup
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
#ifdef ASCO_STACK_INFO
        if (e->sinfo) {
            sinfo_fini(e->sinfo);
            e->sinfo = NULL;
        }
#endif
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


inline cow_entry_t*
cow_find(cow_t* cow, uintptr_t wkey)
{
    int i;
    cow_entry_t* e = cow->buffer;

#ifdef COW_STATS
    ++cow->stats_tr.lkup;
#endif

    for (i = cow->size; i > 0; --i, ++e) {
#ifdef COW_STATS
        ++cow->stats_tr.iter;
#endif
        if (e->wkey == wkey) return e;
    }

#ifdef COW_STATS
    ++cow->stats_tr.miss;
#endif
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

#ifdef ASCO_STACK_INFO
#define SINFO_UPDATE(e, addr) do {                                    \
        if (e->sinfo == NULL) e->sinfo = sinfo_init((void*) addr);    \
        else sinfo_update(e->sinfo, (void*) addr);                    \
    } while(0)
#else
#define SINFO_UPDATE(e, addr)
#endif

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
        SINFO_UPDATE(e, GETWADDR(e->wkey));                             \
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
