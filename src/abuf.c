/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014,2015 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sched.h>
#include "fail.h"

/* ----------------------------------------------------------------------------
 * types and data structures
 * ------------------------------------------------------------------------- */

#include "abuf.h"
#include "debug.h"
#include "config.h"

#ifdef SEI_STACK_INFO
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

#ifdef SEI_STACK_INFO
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
#ifdef SEI_STACK_INFO
    int i;
    for (i = 0; i < abuf->pushed; ++i) {
        abuf_entry_t* e = &abuf->buf[i];
        if (e->sipop) sinfo_fini(e->sipop);
        sinfo_fini(e->sipush);
        e->sipush = NULL;
        e->sipop  = NULL;
    }
#endif
    abuf->pushed = 0;
    abuf->poped  = 0;
}

inline void
abuf_rewind(abuf_t* abuf)
{
    abuf->poped = 0;
}

inline int
abuf_size(abuf_t* abuf)
{
    return abuf->pushed - abuf->poped;
}

#ifdef SEI_STACK_INFO
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
        DLOG3("[%s:%d] reading address %p = x%x (---)\n",               \
              __FILE__, __LINE__, e->addr, ABUF_WVAL(e));               \
        ABUF_SINFO_POP(e, addr);                                        \
        return ABUF_WVAX(e, type, addr);                                \
    }
ABUF_POP(uint8_t)
ABUF_POP(uint16_t)
ABUF_POP(uint32_t)
ABUF_POP(uint64_t)

#if !defined(NDEBUG) && defined(DEBUG)
#define SAVE_NEXT e->next = e + sizeof(abuf_entry_t)
#else
#define SAVE_NEXT
#endif


#ifdef ABUF_DISABLE_REALLOC
#define ABUF_CHECK_SIZE                                               \
        fail_ifn (abuf->pushed < abuf->max_size-1, "no space left");
#else
#define ABUF_CHECK_SIZE                                               \
        if (abuf->pushed == abuf->max_size) {                         \
            abuf->max_size *= 2;                                      \
            abuf->buf = realloc(abuf->buf,                            \
                                abuf->max_size*sizeof(abuf_entry_t)); \
            fail_ifn (abuf->buf != NULL, "no space left");            \
        }
#endif /* ABUF_DISABLE_REALLOC */

#define ABUF_PUSH(type) inline                                  \
    void abuf_push_##type(abuf_t* abuf, type* addr, type value) \
    {                                                           \
        ABUF_CHECK_SIZE;                                        \
        abuf_entry_t* e = &abuf->buf[abuf->pushed++];           \
        e->addr = addr;                                         \
        e->size = sizeof(type);                                 \
        SAVE_NEXT;                                              \
        if (sizeof(type) != sizeof(uint64_t)) ABUF_WVAL(e) = 0; \
        ABUF_WVAX(e, type, addr) = value;                       \
        ABUF_SINFO_PUSH(e, addr);                               \
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

#define ABUF_CONFLICT(e, type) do {                                        \
        type* addr = e->addr;                                              \
        if (*addr != ABUF_WVAX(e, type, addr)) {                           \
            fail_ifn (nentry <= ABUF_MAX_CONFLICTS, "too many conflicts"); \
            entry[nentry++] = e;                                           \
        }                                                                  \
    } while (0)

/**
 * 2-way COW buffer comparison (NORMAL mode)
 * This function is ONLY for N=2 (DMR).
 */
inline void
abuf_cmp_heap(abuf_t* a1, abuf_t* a2)
{
    assert(SEI_DMR_REDUNDANCY == 2);

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

    DLOG1("Number of conflicts: %d\n", nentry);

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

/* ----------------------------------------------------------------------------
 * Check for duplicate entries in buffer (data structure integrity check)
 * This function verifies that there are no duplicate address entries in the buffer.
 * Used when CPU isolation is ON and DMR verification is already done by sei_try_commit.
 * ------------------------------------------------------------------------- */
inline void
abuf_check_duplicates(abuf_t* a1)
{
    assert (a1->poped == 0 && "buffer must be rewound");

    int i, j;
    for (i = 0; i < a1->pushed; ++i) {
        void* addr = a1->buf[i].addr;

        /* Check if this address appears multiple times in the buffer */
        for (j = i + 1; j < a1->pushed; ++j) {
            if (addr == a1->buf[j].addr) {
                fail_ifn(0, "duplicate entry detected in buffer");
            }
        }
    }
}

/* ----------------------------------------------------------------------------
 * Non-destructive heap comparison for DMR verification (CPU isolation ON)
 * Compares a1->wvalue (NEW_0) with current memory values (NEW_1)
 * Returns: 1 if values match, 0 if mismatch detected (allows rollback)
 *
 * This function follows the same logic as abuf_cmp_heap() but returns int
 * instead of aborting, making it suitable for sei_try_commit() which needs
 * to allow rollback on failure.
 *
 * 2-way COW buffer comparison (ROLLBACK mode)
 * This function is ONLY for N=2 (DMR).
 * Non-destructive: saves and restores poped counters.
 * ------------------------------------------------------------------------- */
inline int
abuf_try_cmp_heap(abuf_t* a1, abuf_t* a2)
{
    assert(SEI_DMR_REDUNDANCY == 2);

    /* Save poped counters for non-destructive operation */
    int saved_poped_a1 = a1->poped;
    int saved_poped_a2 = a2->poped;

    /* Verify buffer sizes match */
    if (a1->pushed != a2->pushed) {
        DLOG1("[abuf_try_cmp_heap] buffer sizes differ: %d vs %d\n",
              a1->pushed, a2->pushed);
        a1->poped = saved_poped_a1;
        a2->poped = saved_poped_a2;
        return 0;
    }

    if (a1->poped != a2->poped) {
        DLOG1("[abuf_try_cmp_heap] poped counts differ: %d vs %d\n",
              a1->poped, a2->poped);
        a1->poped = saved_poped_a1;
        a2->poped = saved_poped_a2;
        return 0;
    }

    if (a1->poped != 0) {
        DLOG1("[abuf_try_cmp_heap] buffer not rewound\n");
        a1->poped = saved_poped_a1;
        a2->poped = saved_poped_a2;
        return 0;
    }

    /* Collect conflicts (entries where memory != buffer) */
    abuf_entry_t* entry[ABUF_MAX_CONFLICTS];
    int nentry = 0;

    while (a1->poped < a1->pushed) {
        abuf_entry_t* e1 = &a1->buf[a1->poped++];
        abuf_entry_t* e2 = &a2->buf[a2->poped++];

        /* Check entry sizes match */
        if (e1->size != e2->size) {
            DLOG1("[abuf_try_cmp_heap] entry sizes differ\n");
            a1->poped = saved_poped_a1;
            a2->poped = saved_poped_a2;
            return 0;
        }

        /* Check addresses match */
        if (e1->addr != e2->addr) {
            DLOG1("[abuf_try_cmp_heap] addresses differ: %p vs %p\n",
                  e1->addr, e2->addr);
            a1->poped = saved_poped_a1;
            a2->poped = saved_poped_a2;
            return 0;
        }

        /* Check if memory value matches buffer value (detect conflicts) */
        int conflict = 0;
        switch (e1->size) {
        case sizeof(uint8_t): {
            uint8_t* addr = e1->addr;
            if (*addr != ABUF_WVAX(e1, uint8_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint16_t): {
            uint16_t* addr = e1->addr;
            if (*addr != ABUF_WVAX(e1, uint16_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint32_t): {
            uint32_t* addr = e1->addr;
            if (*addr != ABUF_WVAX(e1, uint32_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint64_t): {
            uint64_t* addr = e1->addr;
            if (*addr != ABUF_WVAX(e1, uint64_t, addr)) {
                conflict = 1;
            }
            break;
        }
        default:
            DLOG1("[abuf_try_cmp_heap] unknown size: %lu\n", e1->size);
            a1->poped = saved_poped_a1;
            a2->poped = saved_poped_a2;
            return 0;
        }

        if (conflict) {
            if (nentry >= ABUF_MAX_CONFLICTS) {
                DLOG1("[abuf_try_cmp_heap] too many conflicts\n");
                a1->poped = saved_poped_a1;
                a2->poped = saved_poped_a2;
                return 0;
            }
            entry[nentry++] = e1;
        }
    }

    DLOG1("[abuf_try_cmp_heap] Number of conflicts: %d\n", nentry);

    if (nentry == 0) {
        /* No conflicts - success */
        a1->poped = saved_poped_a1;
        a2->poped = saved_poped_a2;
        return 1;
    }

    /* Check conflicting entries: ensure each conflict is due to duplicate writes
     * (same address appears multiple times in buffer) */
    int i, j;
    for (i = 0; i < nentry; ++i) {
        abuf_entry_t* ce = entry[i];
        void* addr = ce->addr;

        /* Search for duplicate address in buffer */
        int found_duplicate = 0;
        for (j = a1->pushed - 1; j >= 0; --j) {
            if (addr == a1->buf[j].addr) {
                if (ce != &a1->buf[j]) {
                    /* Found duplicate - this is OK */
                    found_duplicate = 1;
                    break;
                }
            }
        }

        if (!found_duplicate) {
            /* Conflict but no duplicate - this is an error (SDC detected) */
            DLOG1("[abuf_try_cmp_heap] conflict without duplicate at %p\n", addr);
            a1->poped = saved_poped_a1;
            a2->poped = saved_poped_a2;
            return 0;
        }
    }

    /* All conflicts are due to duplicate writes - success */
    a1->poped = saved_poped_a1;
    a2->poped = saved_poped_a2;
    return 1;
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

#ifdef SEI_CPU_ISOLATION
/* ----------------------------------------------------------------------------
 * Rollback: restore old values from abuf[0]
 * ------------------------------------------------------------------------- */

#define ABUF_RESTORE(e, type) do {                                      \
    type* target = (type*) e->addr;                                     \
    type old_value = ABUF_WVAX(e, type, e->addr);                       \
    *target = old_value;                                                \
    DLOG3("[abuf_restore] %p = 0x%lx (size=%lu)\n",                     \
          e->addr, (uint64_t)old_value, e->size);                       \
} while(0)

void
abuf_restore(abuf_t* abuf)
{
    DLOG2("[abuf_restore] restoring %d entries\n", abuf->pushed);

    /* Iterate through all pushed entries and restore old values */
    for (int i = 0; i < abuf->pushed; i++) {
        abuf_entry_t* e = &abuf->buf[i];

        /* Restore the old value (stored in abuf[0]) to memory */
        switch (e->size) {
        case sizeof(uint8_t):
            ABUF_RESTORE(e, uint8_t);
            break;
        case sizeof(uint16_t):
            ABUF_RESTORE(e, uint16_t);
            break;
        case sizeof(uint32_t):
            ABUF_RESTORE(e, uint32_t);
            break;
        case sizeof(uint64_t):
            ABUF_RESTORE(e, uint64_t);
            break;
        default:
            assert (0 && "unknown size in abuf_restore");
        }
    }

    /* Clean the buffer after restoration */
    abuf_clean(abuf);
}

/* ----------------------------------------------------------------------------
 * Non-destructive comparison for SDC detection
 * Returns: 1 if buffers match, 0 if mismatch detected
 * ------------------------------------------------------------------------- */

int
abuf_try_cmp(abuf_t* a1, abuf_t* a2)
{
    int core_id = sched_getcpu();

    if (abuf_size(a1) != abuf_size(a2)) {
        DLOG2("[abuf_try_cmp] buffer sizes differ: %d vs %d\n",
              abuf_size(a1), abuf_size(a2));
        fprintf(stderr, "[DEBUG][core=%d][abuf_try_cmp] SIZE MISMATCH: %d vs %d (pushed: %d vs %d, poped: %d vs %d)\n",
                core_id, abuf_size(a1), abuf_size(a2), a1->pushed, a2->pushed, a1->poped, a2->poped);
        return 0;  /* Mismatch */
    }

    /* Save poped counters to make this truly non-destructive */
    int saved_poped_a1 = a1->poped;
    int saved_poped_a2 = a2->poped;

    abuf_rewind(a1);
    abuf_rewind(a2);

    int result = 1;

    while (a1->poped < a1->pushed) {
        abuf_entry_t* e1 = &a1->buf[a1->poped++];
        abuf_entry_t* e2 = &a2->buf[a2->poped++];

        if (e1->size != e2->size) {
            DLOG2("[abuf_try_cmp] entry sizes differ\n");
            fprintf(stderr, "[DEBUG][core=%d][abuf_try_cmp] ENTRY SIZE MISMATCH at idx %d: %lu vs %lu\n",
                    core_id, a1->poped - 1, e1->size, e2->size);
            result = 0;
            break;
        }

        if (e1->addr != e2->addr) {
            DLOG2("[abuf_try_cmp] addresses differ: %p vs %p\n",
                  e1->addr, e2->addr);
            fprintf(stderr, "[DEBUG][core=%d][abuf_try_cmp] ADDRESS MISMATCH at idx %d: %p vs %p\n",
                    core_id, a1->poped - 1, e1->addr, e2->addr);
            result = 0;
            break;
        }

        if (ABUF_WVAL(e1) != ABUF_WVAL(e2)) {
            DLOG2("[abuf_try_cmp] values differ at %p: 0x%lx vs 0x%lx\n",
                  e1->addr, ABUF_WVAL(e1), ABUF_WVAL(e2));
            fprintf(stderr, "[DEBUG][core=%d][abuf_try_cmp] VALUE MISMATCH at idx %d addr=%p: 0x%lx vs 0x%lx\n",
                    core_id, a1->poped - 1, e1->addr, ABUF_WVAL(e1), ABUF_WVAL(e2));
            result = 0;
            break;
        }
    }

    /* Restore poped counters to original values */
    a1->poped = saved_poped_a1;
    a2->poped = saved_poped_a2;

    return result;
}

#endif /* SEI_CPU_ISOLATION */

/* ----------------------------------------------------------------------------
 * N-way COW buffer comparison functions (N >= 3)
 * ------------------------------------------------------------------------- */
#if SEI_DMR_REDUNDANCY >= 3

/**
 * N-way COW buffer comparison for normal mode (N >= 3)
 *
 * Compares Phase 0 buffer (buffers[0]) with all other phase buffers
 * (buffers[1] ~ buffers[n-1]). Aborts on any mismatch.
 *
 * @param buffers  Array of abuf_t pointers (length n)
 * @param n        Number of phases (SEI_DMR_REDUNDANCY)
 */
inline void
abuf_cmp_heap_nway(abuf_t** buffers, int n)
{
    assert(n >= 3 && n == SEI_DMR_REDUNDANCY);
    assert(buffers != NULL);

    abuf_entry_t* entry[ABUF_MAX_CONFLICTS];
    int nentry = 0;

    /* All buffers must have same pushed/poped counts */
    DLOG1("[abuf_cmp_heap_nway] Buffer state: phase0 pushed=%d poped=%d\n",
          buffers[0]->pushed, buffers[0]->poped);
    for (int i = 1; i < n; i++) {
        DLOG1("[abuf_cmp_heap_nway] Buffer state: phase%d pushed=%d poped=%d\n",
              i, buffers[i]->pushed, buffers[i]->poped);
        if (buffers[0]->pushed != buffers[i]->pushed) {
            DLOG1("[abuf_cmp_heap_nway] ERROR: Buffer size mismatch! phase0=%d vs phase%d=%d\n",
                  buffers[0]->pushed, i, buffers[i]->pushed);
        }
        assert(buffers[0]->pushed == buffers[i]->pushed);
        assert(buffers[0]->poped == buffers[i]->poped);
    }
    assert(buffers[0]->poped == 0);

    /* Entry-by-entry comparison across all phases */
    int entry_index = 0;
    while (entry_index < buffers[0]->pushed) {
        /* Phase 0 entry is the reference */
        abuf_entry_t* e0 = &buffers[0]->buf[entry_index];

        /* Verify all phases have same address and size */
        for (int i = 1; i < n; i++) {
            abuf_entry_t* ei = &buffers[i]->buf[entry_index];
            if (e0->addr != ei->addr) {
                DLOG1("[abuf_cmp_heap_nway] Address mismatch at entry_index=%d: phase0=%p vs phase%d=%p\n",
                      entry_index, e0->addr, i, ei->addr);
                DLOG1("[abuf_cmp_heap_nway] Buffer info: phase0 pushed=%d poped=%d, phase%d pushed=%d poped=%d\n",
                      buffers[0]->pushed, buffers[0]->poped, i, buffers[i]->pushed, buffers[i]->poped);
            }
            fail_ifn(e0->addr == ei->addr, "addresses differ");
            assert(e0->size == ei->size);
        }

        /* Conflict detection: check if memory value != buffer value (Phase 0) */
        int conflict = 0;
        switch (e0->size) {
        case sizeof(uint8_t):
            if (*(uint8_t*)e0->addr != ABUF_WVAX(e0, uint8_t, e0->addr)) {
                conflict = 1;
            }
            break;
        case sizeof(uint16_t):
            if (*(uint16_t*)e0->addr != ABUF_WVAX(e0, uint16_t, e0->addr)) {
                conflict = 1;
            }
            break;
        case sizeof(uint32_t):
            if (*(uint32_t*)e0->addr != ABUF_WVAX(e0, uint32_t, e0->addr)) {
                conflict = 1;
            }
            break;
        case sizeof(uint64_t):
            if (*(uint64_t*)e0->addr != ABUF_WVAX(e0, uint64_t, e0->addr)) {
                conflict = 1;
            }
            break;
        default:
            assert(0 && "unknown case");
        }

        if (conflict) {
            fail_ifn(nentry < ABUF_MAX_CONFLICTS, "too many conflicts");
            entry[nentry++] = e0;
        }

        entry_index++;
    }

    /* Update all phase poped counters */
    for (int i = 0; i < n; i++) {
        buffers[i]->poped = buffers[0]->pushed;
    }

    DLOG1("Number of conflicts: %d\n", nentry);

    if (nentry == 0) return;

    /* Duplicate verification: check conflicting entries */
    int i, j;
    for (i = 0; i < nentry; i++) {
        abuf_entry_t* ce = entry[i];
        void* addr = ce->addr;

        int found = 0;
        for (j = buffers[0]->pushed - 1; j >= 0; --j) {
            if (addr == buffers[0]->buf[j].addr) {
                if (ce != &buffers[0]->buf[j]) {
                    found = 1;  /* Duplicate found */
                    break;
                }
            }
        }
        fail_ifn(found, "not duplicate! error detected");
    }
}

/**
 * N-way COW buffer comparison for ROLLBACK mode (N >= 3)
 *
 * Non-destructive comparison. Returns 1 on success, 0 on mismatch.
 * Does not modify poped counters (saves and restores).
 *
 * @param buffers  Array of abuf_t pointers (length n)
 * @param n        Number of phases (SEI_DMR_REDUNDANCY)
 * @return         1 if all buffers match, 0 otherwise
 */
inline int
abuf_try_cmp_heap_nway(abuf_t** buffers, int n)
{
    assert(n >= 3 && n == SEI_DMR_REDUNDANCY);
    assert(buffers != NULL);

    /* Save poped counters for non-destructive operation */
    int saved_poped[SEI_DMR_REDUNDANCY];
    for (int i = 0; i < n; i++) {
        saved_poped[i] = buffers[i]->poped;
    }

    /* Verify buffer sizes match */
    for (int i = 1; i < n; i++) {
        if (buffers[0]->pushed != buffers[i]->pushed) {
            DLOG1("[abuf_try_cmp_heap_nway] buffer sizes differ: phase0=%d vs phase%d=%d\n",
                  buffers[0]->pushed, i, buffers[i]->pushed);
            for (int k = 0; k < n; k++) {
                buffers[k]->poped = saved_poped[k];
            }
            return 0;
        }
        if (buffers[0]->poped != buffers[i]->poped) {
            DLOG1("[abuf_try_cmp_heap_nway] poped counts differ: phase0=%d vs phase%d=%d\n",
                  buffers[0]->poped, i, buffers[i]->poped);
            for (int k = 0; k < n; k++) {
                buffers[k]->poped = saved_poped[k];
            }
            return 0;
        }
    }

    if (buffers[0]->poped != 0) {
        DLOG1("[abuf_try_cmp_heap_nway] buffer not rewound\n");
        for (int k = 0; k < n; k++) {
            buffers[k]->poped = saved_poped[k];
        }
        return 0;
    }

    /* Collect conflicts (entries where memory != buffer) */
    abuf_entry_t* entry[ABUF_MAX_CONFLICTS];
    int nentry = 0;

    int entry_index = 0;
    while (entry_index < buffers[0]->pushed) {
        abuf_entry_t* e0 = &buffers[0]->buf[entry_index];

        /* Check all phases have same address and size */
        for (int i = 1; i < n; i++) {
            abuf_entry_t* ei = &buffers[i]->buf[entry_index];

            if (e0->size != ei->size) {
                DLOG1("[abuf_try_cmp_heap_nway] entry sizes differ at phase%d\n", i);
                for (int k = 0; k < n; k++) {
                    buffers[k]->poped = saved_poped[k];
                }
                return 0;
            }

            if (e0->addr != ei->addr) {
                DLOG1("[abuf_try_cmp_heap_nway] addresses differ: %p vs %p (phase%d)\n",
                      e0->addr, ei->addr, i);
                for (int k = 0; k < n; k++) {
                    buffers[k]->poped = saved_poped[k];
                }
                return 0;
            }
        }

        /* Check if memory value matches buffer value (detect conflicts) */
        int conflict = 0;
        switch (e0->size) {
        case sizeof(uint8_t): {
            uint8_t* addr = e0->addr;
            if (*addr != ABUF_WVAX(e0, uint8_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint16_t): {
            uint16_t* addr = e0->addr;
            if (*addr != ABUF_WVAX(e0, uint16_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint32_t): {
            uint32_t* addr = e0->addr;
            if (*addr != ABUF_WVAX(e0, uint32_t, addr)) {
                conflict = 1;
            }
            break;
        }
        case sizeof(uint64_t): {
            uint64_t* addr = e0->addr;
            if (*addr != ABUF_WVAX(e0, uint64_t, addr)) {
                conflict = 1;
            }
            break;
        }
        default:
            DLOG1("[abuf_try_cmp_heap_nway] unknown size: %lu\n", e0->size);
            for (int k = 0; k < n; k++) {
                buffers[k]->poped = saved_poped[k];
            }
            return 0;
        }

        if (conflict) {
            if (nentry >= ABUF_MAX_CONFLICTS) {
                DLOG1("[abuf_try_cmp_heap_nway] too many conflicts\n");
                for (int k = 0; k < n; k++) {
                    buffers[k]->poped = saved_poped[k];
                }
                return 0;
            }
            entry[nentry++] = e0;
        }

        entry_index++;
    }

    DLOG1("[abuf_try_cmp_heap_nway] Number of conflicts: %d\n", nentry);

    if (nentry == 0) {
        /* No conflicts - success */
        for (int k = 0; k < n; k++) {
            buffers[k]->poped = saved_poped[k];
        }
        return 1;
    }

    /* Check conflicting entries: ensure each conflict is due to duplicate writes */
    int i, j;
    for (i = 0; i < nentry; i++) {
        abuf_entry_t* ce = entry[i];
        void* addr = ce->addr;

        /* Search for duplicate address in buffer */
        int found_duplicate = 0;
        for (j = buffers[0]->pushed - 1; j >= 0; --j) {
            if (addr == buffers[0]->buf[j].addr) {
                if (ce != &buffers[0]->buf[j]) {
                    /* Found duplicate - this is OK */
                    found_duplicate = 1;
                    break;
                }
            }
        }

        if (!found_duplicate) {
            /* Conflict but no duplicate - this is an error (SDC detected) */
            DLOG1("[abuf_try_cmp_heap_nway] conflict without duplicate at %p\n", addr);
            for (int k = 0; k < n; k++) {
                buffers[k]->poped = saved_poped[k];
            }
            return 0;
        }
    }

    /* All conflicts are due to duplicate writes - success */
    for (int k = 0; k < n; k++) {
        buffers[k]->poped = saved_poped[k];
    }
    return 1;
}

#endif /* SEI_DMR_REDUNDANCY >= 3 */
