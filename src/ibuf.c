/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include <string.h>
#include "ibuf.h"
#include "abuf.h"
#include "crc.h"

/* -----------------------------------------------------------------------------
 * data structures and prototypes
 * -------------------------------------------------------------------------- */

struct ibuf {
    const void* ptr;     /* input message pointer */
    size_t      size;    /* input message size    */
    uint32_t     crc;    /* input crc             */
    uint32_t    tcrc;    /* traversal crc         */
    ibuf_mode_t mode;
    int checked;
};

static int ibuf_correct_on_entry(ibuf_t* ibuf);

/* -----------------------------------------------------------------------------
 * constructor/destructor
 * -------------------------------------------------------------------------- */

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

/* -----------------------------------------------------------------------------
 * interface methods
 * -------------------------------------------------------------------------- */

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

    // check correct message
    return ibuf_correct_on_entry(ibuf);
}

static int
ibuf_correct_on_entry(ibuf_t* ibuf)
{
    uint32_t crc_check = crc_compute(ibuf->ptr, ibuf->size);
    return crc_check == ibuf->crc;
}

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
