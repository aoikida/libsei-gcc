/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <asco.h>
#include "debug.h"

/* ----------------------------------------------------------------------------
 * types and data structures
 * ------------------------------------------------------------------------- */

#include "tbin.h"
#include "talloc.h"
#include "abuf.h"

struct asco {
    int       p;       /* the actual process (0 or 1) */
    abuf_t*   wbuf[2]; /* write buffers               */
    abuf_t*   rbuf;    /* read buffer                 */
    tbin_t*   tbin;    /* trash bin for delayed frees */
    talloc_t* talloc;  /* traversal allocator         */
};

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

asco_t*
asco_init()
{
    asco_t* asco = (asco_t*) malloc(sizeof(asco_t));
    assert(asco);
    asco->wbuf[0] = abuf_init(100000);
    asco->wbuf[1] = abuf_init(100000);
    asco->tbin    = tbin_init(100, NULL);
    asco->talloc  = talloc_init(NULL);
    asco->rbuf    = abuf_init(100000);

    asco->p = -1;

    return asco;
}

void
asco_fini(asco_t* asco)
{
    assert(asco);
    abuf_fini(asco->wbuf[0]);
    abuf_fini(asco->wbuf[1]);
    abuf_fini(asco->rbuf);
    tbin_fini(asco->tbin);
    talloc_fini(asco->talloc);
    free(asco);
}


/* ----------------------------------------------------------------------------
 * traversal control
 * ------------------------------------------------------------------------- */

int
asco_prepare(asco_t* asco, const void* ptr, size_t size, uint32_t crc, int ro)
{
    assert (ptr != NULL);
    assert (asco->p == -1);

    // check input message
    return 1;
}

void
asco_prepare_nm(asco_t* asco)
{
    // empty message
}

void
asco_begin(asco_t* asco)
{
    if (asco->p == -1) {
        DLOG2("First execution\n");
        asco->p = 0;
    }

    if (asco->p == 1) {
        DLOG2("Second execution\n");
    }
}

void
asco_switch(asco_t* asco)
{
    DLOG2("Switch: %d\n", asco->p);
    asco->p = 1;
    DLOG2("Switched: %d\n", asco->p);
    talloc_switch(asco->talloc);
}

void
asco_commit(asco_t* asco)
{
    DLOG2("COMMIT: %d\n", asco->p);
    asco->p = -1;

    //abuf_show(asco->wbuf[0]);
    //abuf_show(asco->wbuf[1]);
    //abuf_show(asco->rbuf);
    abuf_cmp(asco->wbuf[0], asco->wbuf[1]);
    abuf_clean(asco->wbuf[0]);
    abuf_clean(asco->wbuf[1]);
    abuf_clean(asco->rbuf);
    tbin_flush(asco->tbin);
    talloc_clean(asco->talloc);
}

inline int
asco_getp(asco_t* asco)
{
    return asco->p;
}

inline void
asco_setp(asco_t* asco, int p)
{
    asco->p = p;
}

/* ----------------------------------------------------------------------------
 * memory management
 * ------------------------------------------------------------------------- */

inline void*
asco_malloc(asco_t* asco, size_t size)
{
    return talloc_malloc(asco->talloc, size);
}

inline void
asco_free(asco_t* asco, void* ptr)
{
    tbin_add(asco->tbin, ptr, asco->p);
}

void*
asco_calloc(asco_t* asco, size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
    return NULL;
}

/* ----------------------------------------------------------------------------
 * memory management outside traversal
 * ------------------------------------------------------------------------- */

void*
asco_malloc2(asco_t* asco, size_t size)
{
    return malloc(size);
}

void
asco_free2(asco_t* asco, void* ptr1, void* ptr2)
{
    assert (0 && "asco not compiled with HEAP_MODE");
}

inline void*
asco_other(asco_t* asco, void* addr)
{
    assert (0 && "asco not compiled with HEAP_MODE");
    return NULL;
}

/* ----------------------------------------------------------------------------
 * load and stores
 * ------------------------------------------------------------------------- */

#define ASCO_READ(type) inline                                          \
    type asco_read_##type(asco_t* asco, const type* addr)               \
    {                                                                   \
        DLOG3("asco_read_%s(%d) addr = %p", #type, asco->p, addr);      \
        if (asco->p) {                                                  \
            type value = abuf_pop_##type(asco->rbuf, addr);             \
            DLOG3("= %lx, %lx (abuf)\n", (uint64_t) *addr, value);      \
            return value;                                               \
        }                                                               \
        type value = *addr;                                             \
        DLOG3("= %lx, %lx (cor)\n", (uint64_t) *addr, value);           \
        abuf_push_##type(asco->rbuf, (type*) addr, value);              \
        return value;                                                   \
    }
ASCO_READ(uint8_t)
ASCO_READ(uint16_t)
ASCO_READ(uint32_t)
ASCO_READ(uint64_t)

#define ASCO_WRITE(type) inline                                         \
    void asco_write_##type(asco_t* asco, type* addr, type value)        \
    {                                                                   \
        assert (asco->p == 0 || asco->p == 1);                          \
        DLOG3("asco_write_%s(%d): %p <- %llx\n", #type, asco->p,        \
              addr, (uint64_t) value);                                  \
        abuf_t* wbuf = asco->wbuf[asco->p];                             \
        abuf_push_##type(wbuf, addr, value);                            \
        if (asco->p == 0) *addr = value;                                \
    }
ASCO_WRITE(uint8_t)
ASCO_WRITE(uint16_t)
ASCO_WRITE(uint32_t)
ASCO_WRITE(uint64_t)

/* ----------------------------------------------------------------------------
 * output messages
 * ------------------------------------------------------------------------- */

/* asco_output_append and asco_output_done can be called from outside
 * a handler with no effect.
 *
 * asco_output_next can only be called from outside the handler.
 */
void
asco_output_append(asco_t* asco, const void* ptr, size_t size)
{
    if (asco->p == -1) return;
    //obuf_push(asco->obuf, ptr, size);
}

void
asco_output_done(asco_t* asco)
{
    if (asco->p == -1) return;
    //obuf_done(asco->obuf);
}

uint32_t
asco_output_next(asco_t* asco)
{
    assert (asco->p == -1);
    //assert (obuf_size(asco->obuf) > 0 && "no CRC to pop");
    //uint32_t crc = obuf_pop(asco->obuf);

    return 0; //crc;
}
