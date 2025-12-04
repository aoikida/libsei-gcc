/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ibuf.h"
#include "abuf.h"
#include "crc.h"
#include "debug.h"

#ifdef SEI_CPU_ISOLATION
#include <sched.h>
#include "cpu_isolation.h"
#endif

/* CRC redundancy configuration (compile-time) */
#ifndef SEI_CRC_REDUNDANCY
#define SEI_CRC_REDUNDANCY 2
#endif

/* ----------------------------------------------------------------------------
 * data structures and prototypes
 * ------------------------------------------------------------------------- */

struct ibuf {
    const void* ptr;     /* input message pointer */
    size_t      size;    /* input message size    */
    uint32_t     crc;    /* input crc             */
    uint32_t    tcrc;    /* traversal crc         */
    ibuf_mode_t mode;
    int checked;
};

static int ibuf_correct_on_entry(ibuf_t* ibuf);

#ifdef SEI_CPU_ISOLATION
static int ibuf_correct_on_entry_safe(ibuf_t* ibuf);
#endif

/* ----------------------------------------------------------------------------
 * constructor/destructor
 * ------------------------------------------------------------------------- */

ibuf_t*
ibuf_init()
{
    ibuf_t* ibuf = (ibuf_t*) malloc(sizeof(ibuf_t));
    ibuf->checked = 0;
    ibuf->mode = READ_ONLY;
    ibuf->crc  = crc_init();
    return ibuf;
}

void
ibuf_fini(ibuf_t* ibuf)
{
    free(ibuf);
}

/* ----------------------------------------------------------------------------
 * interface methods
 * ------------------------------------------------------------------------- */

int
ibuf_prepare(ibuf_t* ibuf, const void* ptr, size_t size, uint32_t crc,
             ibuf_mode_t mode)
{
    assert (ibuf->checked == 0);
    ibuf->ptr  = ptr;
    ibuf->size = size;
    ibuf->crc  = crc;
    ibuf->mode = mode;

    if (ibuf->ptr == NULL) {
        assert (ibuf->size == 0);
        assert (ibuf->crc == crc_init());
        assert (ibuf->mode == READ_ONLY);
        return 1;
    }

#ifdef SEI_CPU_ISOLATION
    /* CRC verification with automatic core blacklisting and recovery */
    while (1) {
        if (ibuf_correct_on_entry_safe(ibuf)) {
            /* CRC verification successful */
            return 1;
        }

        /* CRC verification failed - SDC detected on current core */
        int current_core = sched_getcpu();
        fprintf(stderr, "[libsei] CRC verification failed on core %d, recovering...\n",
                current_core);

        /* Blacklist the faulty core */
        cpu_isolation_blacklist_current();

        /* Migrate to another available core (exits if all cores blacklisted) */
        cpu_isolation_migrate_current_thread();

        /* Retry CRC verification on the new core */
    }
#else
    /* CPU isolation disabled - use traditional abort-on-error behavior */
    return ibuf_correct_on_entry(ibuf);
#endif
}

static int
ibuf_correct_on_entry(ibuf_t* ibuf)
{
    uint32_t crc_check;

    /* Phase 1: Redundant CRC computation (compile-time redundancy) */
    if (!crc_compute_redundant(ibuf->ptr, ibuf->size, &crc_check, SEI_CRC_REDUNDANCY)) {
        fprintf(stderr, "ERROR: CRC redundancy check failed (redundancy=%d)\n",
                SEI_CRC_REDUNDANCY);
        abort();
    }

    /* Phase 2: Compare with received CRC */
    return crc_check == ibuf->crc;
}

#ifdef SEI_CPU_ISOLATION
/* Non-aborting version of ibuf_correct_on_entry for CPU isolation mode */
static int
ibuf_correct_on_entry_safe(ibuf_t* ibuf)
{
    uint32_t crc_check;

    /* Phase 1: Redundant CRC computation (compile-time redundancy) */
    if (!crc_compute_redundant(ibuf->ptr, ibuf->size, &crc_check, SEI_CRC_REDUNDANCY)) {
        /* CRC redundancy check failed - core may be faulty */
        DLOG1("[ibuf] CRC redundancy check failed (core %d, redundancy=%d)\n",
              sched_getcpu(), SEI_CRC_REDUNDANCY);
        return 0;  /* Return error instead of abort */
    }

    /* Phase 2: Compare with received CRC */
    if (crc_check != ibuf->crc) {
        DLOG1("[ibuf] CRC mismatch: expected 0x%08x, got 0x%08x (core %d)\n",
              ibuf->crc, crc_check, sched_getcpu());
        return 0;
    }

    return 1;  /* Success */
}
#endif

void
ibuf_switch(ibuf_t* ibuf)
{
    if (ibuf->mode == READ_ONLY) {
        // nothing to do
    } else {
        // save crc of modified message
        ibuf->tcrc = crc_compute(ibuf->ptr, ibuf->size);
    }
}

int
ibuf_correct(ibuf_t* ibuf)
{
    if (ibuf->mode == READ_ONLY) {
        if (ibuf->ptr == NULL) {
            assert (ibuf->size == 0);
            assert (ibuf->crc == crc_init());
            assert (ibuf->mode == READ_ONLY);
            return 1;
        }

        // message was not modified, so CRC should still hold
        uint32_t crc_check = crc_compute(ibuf->ptr, ibuf->size);
        return crc_check == ibuf->crc;
    } else {
        // calculate crc of modified message
        uint32_t crc_check = crc_compute(ibuf->ptr, ibuf->size);
        return crc_check == ibuf->tcrc;
    }
}
