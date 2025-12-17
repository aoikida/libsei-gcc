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
#include "sei.h"
#include "cow.h"

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
#ifdef SEI_CRC_MIGRATE_CORES
        int crc_phase0_core = sched_getcpu();  /* Record phase0 core for blacklisting */
#endif

        int result = ibuf_correct_on_entry_safe(ibuf);

        if (result == 2) {
            /* CRC verification successful */
            return 1;
        }

        if (result == 1) {
            /* Type B error: Received message corrupted
             * Do NOT retry - return failure to application */
            //fprintf(stderr, "[libsei] Received message corrupted (CRC mismatch), discarding\n");
            return 0;
        }

        /* result == 0: Type A error (SDC detected) - retry with core blacklisting */
        int current_core = sched_getcpu();
        //fprintf(stderr, "[libsei] Type A error: SDC detected on core %d, recovering...\n",current_core);

        /* Blacklist the faulty core(s) */
#ifdef SEI_CRC_MIGRATE_CORES
        /* Cross-core mode: blacklist both phase0 and phase1 cores */
        cpu_isolation_blacklist_core(crc_phase0_core);  /* Phase0 core */
        cpu_isolation_blacklist_core(current_core);      /* Phase1 core */
        //fprintf(stderr, "[libsei] Blacklisted cores %d and %d (CRC cross-core failure)\n",crc_phase0_core, current_core);
#else
        /* Traditional mode: blacklist only the current core */
        cpu_isolation_blacklist_current();
#endif

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
/* Non-aborting version of ibuf_correct_on_entry for CPU isolation mode
 * Return values:
 *   2 = CRC verification successful
 *   1 = Received message corrupted (Type B error - do not retry)
 *   0 = SDC detected (Type A error - retry with core blacklisting)
 */
static int
ibuf_correct_on_entry_safe(ibuf_t* ibuf)
{
    uint32_t crc_check;

#ifdef SEI_CRC_MIGRATE_CORES
    /* ===== Cross-Core CRC Mode ===== */

    /* Phase 0: Compute CRC on current core */
    int phase0_core = sched_getcpu();
    uint32_t crc_phase0 = crc_compute(ibuf->ptr, ibuf->size);

    DLOG1("[ibuf] CRC Phase0: computed 0x%08x on core %d\n", crc_phase0, phase0_core);

    /* Migrate to a different core for Phase 1
     * Note: This is called before transaction starts, so no need to save/restore sei->p */
    int phase1_core = cpu_isolation_migrate_excluding_core(phase0_core);
    //fprintf(stderr, "[ibuf] CRC migration: core %d (phase0) -> core %d (phase1)\n",phase0_core, phase1_core);

    /* Phase 1: Compute CRC on different core */
    uint32_t crc_phase1 = crc_compute(ibuf->ptr, ibuf->size);

    DLOG1("[ibuf] CRC Phase1: computed 0x%08x on core %d\n", crc_phase1, phase1_core);

    /* DMR Verification: Compare Phase0 and Phase1 results */
    if (crc_phase0 != crc_phase1) {
        DLOG1("[ibuf] CRC cross-core mismatch: phase0=0x%08x (core %d) != phase1=0x%08x (core %d)\n",
              crc_phase0, phase0_core, crc_phase1, phase1_core);
        return 0;  /* SDC detected */
    }

    crc_check = crc_phase0;  /* Use Phase0 result (both are identical) */

#else
    /* ===== Traditional Redundancy Mode (same core) - UNCHANGED ===== */

    /* Phase 1: Redundant CRC computation (compile-time redundancy) */
    if (!crc_compute_redundant(ibuf->ptr, ibuf->size, &crc_check, SEI_CRC_REDUNDANCY)) {
        /* CRC redundancy check failed - core may be faulty */
        DLOG1("[ibuf] CRC redundancy check failed (core %d, redundancy=%d)\n",
              sched_getcpu(), SEI_CRC_REDUNDANCY);
        return 0;  /* Return error instead of abort */
    }
#endif

    /* Phase 2: Compare with received CRC (common to both modes) */
    if (crc_check != ibuf->crc) {
        DLOG1("[ibuf] Type B error: Received message corrupted (expected 0x%08x, got 0x%08x, core %d)\n",
              ibuf->crc, crc_check, sched_getcpu());
        return 1;  /* Message corruption - do not retry */
    }

    return 2;  /* Success */
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
