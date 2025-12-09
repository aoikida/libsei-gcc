/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include "obuf.h"
#include "abuf.h"
#include "crc.h"

/* ----------------------------------------------------------------------------
 * types, data structures and definitions
 * ------------------------------------------------------------------------- */

/* N-way DMR redundancy configuration */
#ifndef SEI_DMR_REDUNDANCY
#define SEI_DMR_REDUNDANCY 2
#endif

typedef struct obuf_entry {
    size_t size;
    uint32_t crc;
    int done;
} obuf_entry_t;

typedef struct obuf_queue {
    obuf_entry_t* entries;
    int head;
    int tail;
} obuf_queue_t;

struct obuf {
    obuf_queue_t queue[SEI_DMR_REDUNDANCY];
    int p;
};

#define MAX_MSGS 100

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

obuf_t*
obuf_init(int max_msgs)
{
    obuf_t* obuf = (obuf_t*) malloc(sizeof(obuf_t));

    /* Allocate and initialize N queues (one per phase) */
    for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
        obuf->queue[p].entries = malloc(sizeof(obuf_entry_t)*MAX_MSGS);
        obuf->queue[p].head = 0;
        obuf->queue[p].tail = 0;

        /* Initialize all entries in this queue */
        for (int i = 0; i < MAX_MSGS; ++i) {
            obuf->queue[p].entries[i].size = 0;
            obuf->queue[p].entries[i].crc  = crc_init();
            obuf->queue[p].entries[i].done = 0;
        }
    }

    obuf->p = 0;
    return obuf;
}

void
obuf_fini(obuf_t* obuf)
{
    /* Free all N queues */
    for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
        free(obuf->queue[p].entries);
    }
    free(obuf);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

void
obuf_close(obuf_t* obuf)
{
    /* Sequential phase advancement with wraparound */
    obuf->p = (obuf->p + 1) % SEI_DMR_REDUNDANCY;
}

inline void
obuf_push(obuf_t* obuf, const void* ptr, size_t size)
{
    //fprintf(stderr,"CRC of %s\n", (char*) ptr);
    obuf_queue_t* queue = &obuf->queue[obuf->p];
    obuf_entry_t* e = &queue->entries[queue->tail % MAX_MSGS];
    assert (!e->done);

    // append value and increment size
#ifdef COW_WB
    e->crc   = txcrc_append(e->crc, (const char*) ptr, size);
#else
    e->crc   = crc_append(e->crc, (const char*) ptr, size);
#endif
    e->size += size;
}

inline void
obuf_done(obuf_t* obuf)
{
    obuf_queue_t* queue = &obuf->queue[obuf->p];
    obuf_entry_t* e = &queue->entries[queue->tail % MAX_MSGS];
    assert (!e->done);

    assert (e->size > 0 && "message with no content");

    // append size to CRC and close
    e->crc = crc_append_len(e->crc, e->size);
    //fprintf(stderr,"CRC was %u\n", e->crc);
    e->crc = crc_close(e->crc);

    //fprintf(stderr,"CRC is %u\n", e->crc);
    // mark as done
    e->done = 1;

    // add new element
    queue->tail++;
    assert (queue->tail - queue->head <= MAX_MSGS);
}

inline uint32_t
obuf_pop(obuf_t* obuf)
{
    /* N-way verification: all queues must have messages */
    for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
        assert (obuf->queue[p].head < obuf->queue[p].tail);
    }

    /* Get entries from all phases and increment head pointers */
    obuf_entry_t* entries[SEI_DMR_REDUNDANCY];
    for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
        obuf_queue_t* q = &obuf->queue[p];
        entries[p] = &q->entries[q->head++ % MAX_MSGS];
    }

    /* N-way verification: all entries must match Phase 0 */
    for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
        assert (entries[0]->size == entries[p]->size);
        assert (entries[0]->done == entries[p]->done);
        assert (entries[0]->crc  == entries[p]->crc);
    }

    /* Save CRC from Phase 0 as reference */
    uint32_t crc = entries[0]->crc;

    /* Reset all entries */
    for (int p = 0; p < SEI_DMR_REDUNDANCY; p++) {
        entries[p]->size = 0;
        entries[p]->done = 0;
        entries[p]->crc  = crc_init();
    }

    return crc;
}


inline int
obuf_size(obuf_t* obuf)
{
    /* N-way verification: all queues must have same head/tail */
    for (int p = 1; p < SEI_DMR_REDUNDANCY; p++) {
        assert (obuf->queue[0].head == obuf->queue[p].head);
        assert (obuf->queue[0].tail == obuf->queue[p].tail);
    }

    return obuf->queue[0].tail - obuf->queue[0].head;
}
