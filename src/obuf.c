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
    obuf_queue_t queue[2];
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
    obuf->queue[0].entries = malloc(sizeof(obuf_entry_t)*MAX_MSGS);
    obuf->queue[1].entries = malloc(sizeof(obuf_entry_t)*MAX_MSGS);

    // initialize
    int i;
    for (i = 0; i < MAX_MSGS; ++i) {
        obuf->queue[0].entries[i].size = 0;
        obuf->queue[0].entries[i].crc  = crc_init();
        obuf->queue[0].entries[i].done = 0;
        obuf->queue[0].head = 0;
        obuf->queue[0].tail = 0;

        obuf->queue[1].entries[i].size = 0;
        obuf->queue[1].entries[i].crc  = crc_init();
        obuf->queue[1].entries[i].done = 0;
        obuf->queue[1].head = 0;
        obuf->queue[1].tail = 0;
    }

    obuf->p = 0;
    return obuf;
}

void
obuf_fini(obuf_t* obuf)
{
    free(obuf->queue[0].entries);
    free(obuf->queue[1].entries);
    free(obuf);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

void
obuf_close(obuf_t* obuf)
{
    obuf->p = 1 - obuf->p;
}

inline void
obuf_push(obuf_t* obuf, const void* ptr, size_t size)
{
    //fprintf(stderr,"CRC of %s\n", (char*) ptr);
    obuf_queue_t* queue = &obuf->queue[obuf->p];
    obuf_entry_t* e = &queue->entries[queue->tail % MAX_MSGS];
    assert (!e->done);

    // append value and increment size
    e->crc   = crc_append(e->crc, (const char*) ptr, size);
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
    assert (obuf->queue[0].head < obuf->queue[0].tail);
    assert (obuf->queue[1].head < obuf->queue[1].tail);

    obuf_queue_t* q1 = &obuf->queue[0];
    obuf_queue_t* q2 = &obuf->queue[1];

    obuf_entry_t* e1 = &q1->entries[q1->head++ % MAX_MSGS];
    obuf_entry_t* e2 = &q2->entries[q2->head++ % MAX_MSGS];

    assert (e1->size == e2->size);
    assert (e1->done == e2->done);
    assert (e1->crc  == e2->crc);

    uint32_t crc = e1->crc;

    e1->size = e2->size = 0;
    e1->done = e2->done = 0;
    e1->crc  = e2->crc  = crc_init();

    return crc;
}


inline int
obuf_size(obuf_t* obuf)
{
    assert (obuf->queue[0].head == obuf->queue[1].head);
    assert (obuf->queue[0].tail == obuf->queue[1].tail);

    return obuf->queue[0].tail - obuf->queue[1].head;
}
