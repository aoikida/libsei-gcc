/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <stdio.h>
#include <assert.h>
#include <string.h>

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

#include "fail.h"
#include "cow.h"
#include "heap.h"
#include "debug.h"
#ifdef ASCO_STACK_INFO
#include "sinfo.h"
#endif

#define COW_POW 4
#define COW_MAX (1<<COW_POW)

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
    cow_entry_t* table[COW_MAX];
    int          sizes[COW_MAX];
    int max_size;
    heap_t *heap;

#ifdef COW_STATS
    struct {
        uint64_t miss;
        uint64_t size;
        uint64_t iter;
        uint64_t lkup;
        uint64_t count;
    } stats;

    struct {
        uint64_t size;
        uint64_t miss;
        uint64_t iter;
        uint64_t lkup;
    } stats_tr;
#endif
};

#include "cow_helpers.h"

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

cow_t*
cow_init(heap_t *heap, int max_size)
{
#if MODE == 1  // HEAP_MODE only
    assert(heap && "For heap mode the cow needs its heap pointer!");
#endif

    cow_t* cow = (cow_t*) malloc(sizeof(cow_t));
    assert(cow);

    cow->max_size = max_size;
    cow->heap = heap;

    int i;
    for (i = 0; i < COW_MAX; ++i) {
        cow->table[i] = (cow_entry_t*) malloc(max_size*sizeof(cow_entry_t));
        assert (cow->table[i]);
        bzero(cow->table[i], sizeof(cow_entry_t));
        cow->sizes[i] = 0;
    }


#ifdef COW_STATS
    cow->stats.size = 0;
    cow->stats.iter = 0;
    cow->stats.miss = 0;
    cow->stats.lkup = 0;
    cow->stats.count = 0;
    cow->stats_tr.miss = 0;
    cow->stats_tr.iter = 0;
    cow->stats_tr.lkup = 0;
    cow->stats_tr.size = 0;
#endif

    return cow;
}

void
cow_fini(cow_t* cow)
{
    int i;
    for (i = 0; i < COW_MAX; ++i)
        free(cow->table[i]);
    free(cow);
}

/* -----------------------------------------------------------------------------
 * main interface methods
 * -------------------------------------------------------------------------- */

void
cow_apply_cmp(cow_t* cow1, cow_t* cow2)
{
    int i;
    for (i = 0; i < COW_MAX; ++i) {
        fail_if (cow1->sizes[i] == cow2->sizes[i], "cows with different sizes");

        if (cow1->sizes[i] == 0) continue;

        int j;
        for (j = 0; j < cow1->sizes[i]; ++j) {
            cow_entry_t* e1 = &cow1->table[i][j];
            cow_entry_t* e2 = &cow2->table[i][j];

            fail_if(e1->wkey == e2->wkey,
                    "entries point to different addresses");


#ifndef COWBACK
            addr_t v1 = WVAL(e1);
            addr_t v2 = WVAL(e2);
            fail_if(v1 == v2, "cow entries differ (value)");
            *(addr_t*) GETWADDR(cow1->heap, e1->wkey) = v1;
#else  /* ! COWBACK */
            addr_t v1 = WVAL(e1);
            addr_t v2 = *((addr_t*) GETWADDR(cow1->heap, e1->wkey));
            fail_if (v1 == v2, "cow entries differ (value)");
#endif /* ! COWBACK */

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
        cow1->stats_tr.size += cow1->sizes[i];
#endif
        // cleanup
        cow1->sizes[i] = cow2->sizes[i] = 0;
    }

#ifdef COW_STATS
    cow1->stats.size   += cow1->stats_tr.size;
    cow1->stats.miss   += cow1->stats_tr.miss;
    cow1->stats.iter   += cow1->stats_tr.iter;
    cow1->stats.lkup   += cow1->stats_tr.lkup;
    cow1->stats_tr.miss = 0;
    cow1->stats_tr.iter = 0;
    cow1->stats_tr.lkup = 0;
    cow1->stats_tr.size = 0;
    ++cow1->stats.count;

    if (cow1->stats.count % 1000 == 0) {
        printf("mean cow size = %f\n", cow1->stats.size*1.0/cow1->stats.count);
        printf("mean cow miss = %f\n", cow1->stats.miss*1.0/cow1->stats.count);
        printf("mean cow iter = %f\n", cow1->stats.iter*1.0/cow1->stats.count);
        printf("mean cow lkup = %f\n", cow1->stats.lkup*1.0/cow1->stats.count);
    }
#endif
}

void
cow_apply_heap(cow_t* cow1, cow_t* cow2)
{
    int i;
    for (i = 0; i < COW_MAX; ++i) {
        fail_if (cow1->sizes[i] == cow2->sizes[i], "different sizes");

        if (cow1->sizes[i] == 0) continue;

        int j;
        for (j = 0; j < cow1->sizes[i]; ++j) {
            cow_entry_t* e1 = &cow1->table[i][j];
            cow_entry_t* e2 = &cow2->table[i][j];

            fail_if (GETWADDR(cow1->heap, e1->wkey) !=
                     GETWADDR(cow2->heap, e2->wkey),
                     "cow entries point to same address");

            addr_t v1 = WVAL(e1);
            addr_t v2 = WVAL(e2);
            if (v1 != v2) {
                fail_if (heap_rel(cow1->heap, (void*) v1) ==
                         heap_rel(cow2->heap, (void*) v2),
                         "cow values differ and are not pointers");
            }

            *(addr_t*) GETWADDR(cow1->heap, e1->wkey) = v1;
            *(addr_t*) GETWADDR(cow2->heap, e2->wkey) = v2;

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
        cow1->stats_tr.size += cow1->sizes[i];
#endif

        // cleanup
        cow1->sizes[i] = cow2->sizes[i] = 0;
    }

#ifdef COW_STATS
    // stats
    cow1->stats.size   += cow1->stats_tr.size;
    cow1->stats.miss   += cow1->stats_tr.miss;
    cow1->stats.iter   += cow1->stats_tr.iter;
    cow1->stats.lkup   += cow1->stats_tr.lkup;
    cow1->stats_tr.miss = 0;
    cow1->stats_tr.iter = 0;
    cow1->stats_tr.lkup = 0;
    cow1->stats_tr.size = 0;
    ++cow1->stats.count;

    if (cow1->stats.count % 1000 == 0) {
        printf("mean cow size = %f\n", cow1->stats.size*1.0/cow1->stats.count);
        printf("mean cow miss = %f\n", cow1->stats.miss*1.0/cow1->stats.count);
        printf("mean cow iter = %f\n", cow1->stats.iter*1.0/cow1->stats.count);
        printf("mean cow lkup = %f\n", cow1->stats.lkup*1.0/cow1->stats.count);
    }
#endif
}

void
cow_apply(cow_t* cow)
{
    int i;
    for (i = 0; i < COW_MAX; ++i) {
        int j;
        for (j = 0; j < cow->sizes[i]; ++j) {
            cow_entry_t* e = &cow->table[i][j];
            uintptr_t addr = GETWADDR(cow->heap, e->wkey);
            *((addr_t*) addr) = WVAL(e);
#ifdef ASCO_STACK_INFO
            if (e->sinfo) {
                sinfo_fini(e->sinfo);
                e->sinfo = NULL;
            }
#endif
        }
        cow->sizes[i] = 0;
    }
}

void
cow_swap(cow_t* cow)
{
#ifndef COWBACK
    assert (0 && "only supported with COWBACK");
#endif

    int i;
    for (i = 0; i < COW_MAX; ++i) {
        int j;
        for (j = 0; j < cow->sizes[i]; ++j) {
            cow_entry_t* e = &cow->table[i][j];
            uintptr_t addr = GETWADDR(cow->heap, e->wkey);
            addr_t val = *((addr_t*) addr);
            *((addr_t*) addr) = WVAL(e);
            WVAL(e) = val;
        }
    }
}

static inline void
cow_realloc(cow_t* cow)
{
    assert (0 && "not implemented");
}

cow_entry_t*
cow_find(cow_t* cow, uintptr_t wkey)
{
    int i;
#ifdef COW_STATS
    ++cow->stats_tr.lkup;
#endif

    for (i = 0; i < cow->sizes[HASH(wkey)]; ++i) {
        cow_entry_t* e = &cow->table[HASH(wkey)][i];

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
#ifdef COWBACK
#define COW_READ(type) inline                                           \
    type cow_read_##type(cow_t* cow, const type* addr)                  \
    {                                                                   \
        return *addr;                                                   \
    }
#else
#define COW_READ(type) inline                                           \
    type cow_read_##type(cow_t* cow, const type* addr)                  \
    {                                                                   \
        cow_entry_t* e = cow_find(cow, GETWKEY(cow->heap, addr));       \
        if (e == NULL) {                                                \
            return *addr;                                               \
        }                                                               \
        DLOG3("read: entry wkey=%p wvalue=0x%"PRIx64"\n", (void*)e->wkey\
                , e->wvalue._uint64_t.value[0]);                        \
        if (TYPEMASK(addr, type) == 0) {                                \
            return WVAX(e, type, addr);                                 \
        } else {                                                        \
            assert (0 && "cant handle unaligned accesses");             \
        }                                                               \
    }
#endif
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
                                                                        \
        if(((uintptr_t) addr & (sizeof(type) - 1)) != 0) {              \
            assert (0 && "cant handle unaligned accesses");             \
        }                                                               \
        cow_entry_t* e = cow_find(cow, GETWKEY(cow->heap, addr));       \
        if (!e) {                                                       \
            int key = HASH(GETWKEY(cow->heap, addr));                   \
            DLOG3("key=%d\n",key);                                      \
            e = &cow->table[key][cow->sizes[key]++];                    \
            assert (e);                                                 \
            e->wkey = GETWKEY(cow->heap, addr);                         \
            WVAL(e) = *(addr_t*) GETWADDR(cow->heap, e->wkey);          \
            DLOG3("[%s:%d] creating new entry x%x = x%x  (x%x)\n",      \
                            __FILE__, __LINE__, (unsigned int) e->wkey, \
                         (unsigned int) WVAL(e), (unsigned int) value); \
        }                                                               \
        WVAX(e, type, addr) = value;                                    \
        DLOG3("write: entry wkey=%p wvalue=0x%"PRIx64"\n",              \
                (void*)e->wkey, e->wvalue._uint64_t.value[0]);          \
        SINFO_UPDATE(e, GETWADDR(cow->heap, e->wkey));                  \
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
    int i;
    DLOG3("----------\n");
    DLOG3("COW BUFFER %p:\n", cow);
    for (i = 0; i < COW_MAX; ++i) {
        int j;
        for (j = 0; j < cow->sizes[i]; ++j) {
            DLOG3("addr = %p value = %"PRIu64" x%"PRIx64" \n",
                    (void*) cow->table[i][j].wkey,
                    cow->table[i][j].wvalue._uint64_t.value[0],
                    cow->table[i][j].wvalue._uint64_t.value[0]);
        }
    }
    DLOG3("----------\n");
}
