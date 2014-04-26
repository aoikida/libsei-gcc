/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "fail.h"

/* ----------------------------------------------------------------------------
 * types and data structures
 * ------------------------------------------------------------------------- */

#include "abuf.h"
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
} abuf_word_t;

typedef struct abuf_entry {
    uintptr_t   wkey;
    abuf_word_t wvalue;

    uint64_t size;
    void* addr;

#ifdef DEBUG
    struct abuf_entry* next;
#endif

#ifdef ASCO_STACK_INFO
    sinfo_t* sipop;
    sinfo_t* sipush;
#endif
} abuf_entry_t;

struct abuf {
    abuf_entry_t* buf;
    int max_size;
    int pushed;
    int poped;
#ifdef ABUF_STATS
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

/* ----------------------------------------------------------------------------
 * helper macros
 * ------------------------------------------------------------------------- */

#define ABUF_MAX_CONFLICTS 200

#define ABUF_TYPEMASK(addr, type) ( (uintptr_t) addr & (sizeof(type) - 1))
#define ABUF_PICKMASK(addr, type) (((uintptr_t) addr & 0x07)    \
                                   >> (sizeof(type) >> 1))
#define ABUF_WVAL(e) (e->wvalue._uint64_t.value[0])

#define ABUF_WVAX(e, type, addr) (e->wvalue._##type.value       \
                                  [ABUF_PICKMASK(addr,type)])

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

abuf_t*
abuf_init(int max_size)
{
    abuf_t* abuf = (abuf_t*) malloc(sizeof(abuf_t));
    assert(abuf);

    abuf->max_size = max_size;
    abuf->pushed   = 0;
    abuf->poped    = 0;

    abuf->buf = (abuf_entry_t*) malloc(max_size*sizeof(abuf_entry_t));
    assert (abuf->buf);
    bzero(abuf->buf, max_size*sizeof(abuf_entry_t));

#ifdef ABUF_STATS
    abuf->stats.size = 0;
    abuf->stats.iter = 0;
    abuf->stats.miss = 0;
    abuf->stats.lkup = 0;
    abuf->stats.count = 0;
    abuf->stats_tr.miss = 0;
    abuf->stats_tr.iter = 0;
    abuf->stats_tr.lkup = 0;
    abuf->stats_tr.size = 0;
#endif
    return abuf;
}

void
abuf_fini(abuf_t* abuf)
{
    free(abuf->buf);
    free(abuf);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

inline void
abuf_clean(abuf_t* abuf)
{
#ifdef ASCO_STACK_INFO
    int i;
    for (i = 0; i < abuf->pushed; ++i) {
        abuf_entry_t* e = &abuf->buf[i];
        if (e->sipop) sinfo_fini(e->sipop);
        sinfo_fini(e->sipush);
        e->sipush = NULL;
    }
#endif
    abuf->pushed = 0;
    abuf->poped  = 0;
}

inline int
abuf_size(abuf_t* abuf)
{
    return abuf->pushed - abuf->poped;
}

#ifdef ASCO_STACK_INFO
#define ABUF_SINFO_POP(e, addr) do {                                    \
        if (e->sipop == NULL) e->sipop = sinfo_init((void*) addr);      \
        else sinfo_update(e->sipop, (void*) addr);                      \
    } while(0)
#define ABUF_SINFO_PUSH(e, addr) do {                                   \
        if (e->sipush == NULL) e->sipush = sinfo_init((void*) addr);    \
        else sinfo_update(e->sipush, (void*) addr);                     \
    } while(0)
#else
#define ABUF_SINFO_POP(e, addr)
#define ABUF_SINFO_PUSH(e, addr)
#endif


#define ABUF_POP(type) inline                                           \
    type abuf_pop_##type(abuf_t* abuf, const type* addr)                \
    {                                                                   \
        assert (abuf->poped < abuf->pushed && "no entry to be read");   \
        abuf_entry_t* e = &abuf->buf[abuf->poped++];                    \
        fail_ifn(e->addr == addr, "reading wrong address");             \
        assert (e->size == sizeof(type) && "reading wrong size");       \
        DLOG3("[%s:%d] reading address %p = x%x (x%x)\n",               \
              __FILE__, __LINE__, e->addr, ABUF_WVAL(e), *addr);        \
        ABUF_SINFO_POP(e, addr);                                        \
        return ABUF_WVAX(e, type, addr);                                \
    }
ABUF_POP(uint8_t)
ABUF_POP(uint16_t)
ABUF_POP(uint32_t)
ABUF_POP(uint64_t)

#ifndef NDEBUG
#define SAVE_NEXT e->next = e + sizeof(abuf_entry_t)
#else
#define SAVE_NEXT
#endif

#define ABUF_PUSH(type) inline                                          \
    void abuf_push_##type(abuf_t* abuf, type* addr, type value)         \
    {                                                                   \
        assert (abuf->pushed < abuf->max_size-1 && "no space left");    \
        abuf_entry_t* e = &abuf->buf[abuf->pushed++];                   \
        e->addr = addr;                                                 \
        e->size = sizeof(type);                                         \
        SAVE_NEXT;                                                      \
        if (sizeof(type) != sizeof(uint64_t)) ABUF_WVAL(e) = 0;         \
        ABUF_WVAX(e, type, addr) = value;                               \
        ABUF_SINFO_PUSH(e, addr);                                       \
    }
ABUF_PUSH(uint8_t)
ABUF_PUSH(uint16_t)
ABUF_PUSH(uint32_t)
ABUF_PUSH(uint64_t)


inline void*
abuf_pop(abuf_t* abuf, uint64_t* value)
{
    assert (abuf->poped < abuf->pushed && "no entry to be read");
    abuf_entry_t* e = &abuf->buf[abuf->poped++];
    assert (e->size == sizeof(uint64_t) && "reading wrong size");
    DLOG3("[%s:%d] reading address %p = x%x\n",
          __FILE__, __LINE__, e->addr, ABUF_WVAL(e));
    ABUF_SINFO_POP(e, e->addr);

    *value = ABUF_WVAX(e, uint64_t, e->addr);
    return e->addr;
}

inline void
abuf_push(abuf_t* abuf, void* addr, uint64_t value)
{
    abuf_push_uint64_t(abuf, (uint64_t*) addr, value);
}


inline void
abuf_cmp(abuf_t* a1, abuf_t* a2)
{
    fail_ifn(a1->pushed == a2->pushed, "differ nb elements");
    fail_ifn(a1->poped == a2->poped, "differ nb poped elements");
    assert (a1->poped == 0 && "elements were poped");
    while (a1->poped < a1->pushed) {
        abuf_entry_t* e1 = &a1->buf[a1->poped++];
        abuf_entry_t* e2 = &a2->buf[a2->poped++];
        assert (e1->size == e2->size);
        fail_ifn(e1->addr == e2->addr, "addresses differ");
        fail_ifn(ABUF_WVAL(e1) == ABUF_WVAL(e2), "values differ");
    }
}

#define ABUF_CONFLICT(e, type) do {                     \
        type* addr = e->addr;                           \
        if (*addr != ABUF_WVAX(e, type, addr)) {        \
            assert (nentry <= ABUF_MAX_CONFLICTS);      \
            entry[nentry++] = e;                        \
        }                                               \
    } while (0)

inline void
abuf_cmp_heap(abuf_t* a1, abuf_t* a2)
{
    abuf_entry_t* entry[ABUF_MAX_CONFLICTS];
    int nentry = 0; // number of potential conflicts

    assert (a1->pushed == a2->pushed);
    assert (a1->poped == a2->poped);
    assert (a1->poped == 0);

    while (a1->poped < a1->pushed) {
        abuf_entry_t* e1 = &a1->buf[a1->poped++];
        abuf_entry_t* e2 = &a2->buf[a2->poped++];

        assert (e1->size == e2->size);
        fail_ifn(e1->addr == e2->addr, "addresses differ");

        switch (e1->size) {
        case sizeof(uint8_t):
            ABUF_CONFLICT(e1, uint8_t);
            break;
        case sizeof(uint16_t):
            ABUF_CONFLICT(e1, uint16_t);
            break;
        case sizeof(uint32_t):
            ABUF_CONFLICT(e1, uint32_t);
            break;
        case sizeof(uint64_t):
            ABUF_CONFLICT(e1, uint64_t);
            break;
        default:
            assert (0 && "unknown case");
        }
    }

    if (nentry == 0) return;

    // check conflicting entries
    int i, j;
    for (i = 0; i < nentry; ++i) {
        abuf_entry_t* ce = entry[i];
        void* addr = ce->addr;

        for (j = a1->pushed-1; j >= 0; --j) {
            //++loop;
            if (addr == a1->buf[j].addr) {
                fail_ifn(ce != &a1->buf[j], "not duplicate! error detected");
                break;
            }
        }
        assert (j >= 0 && "ce not found");
    }
    // printf ("nentry= %d pushed= %d search= %d\n", nentry, a1->pushed, loop);
}


#define ABUF_SWAP(e, type) do {                         \
        type* taddr = (type*) e->addr;                  \
        type value = ABUF_WVAX(e, type, e->addr);       \
        ABUF_WVAX(e, type, e->addr) = *taddr;           \
        *taddr = value;                                 \
    } while(0)

inline void
abuf_swap(abuf_t* abuf)
{
    assert (abuf->poped == 0);
    int i;
    for (i = abuf->pushed-1; i >= 0; --i) {
        abuf_entry_t* e = &abuf->buf[i];

        switch (e->size) {
        case sizeof(uint8_t):
            ABUF_SWAP(e, uint8_t);
            break;
        case sizeof(uint16_t):
            ABUF_SWAP(e, uint16_t);
            break;
        case sizeof(uint32_t):
            ABUF_SWAP(e, uint32_t);
            break;
        case sizeof(uint64_t):
            ABUF_SWAP(e, uint64_t);
            break;
        default:
            assert (0 && "unknown case");
        }
    }
}
