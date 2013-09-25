/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include "cfc.h"

void
cfc_reset(cfc_t* cfc)
{
    assert(cfc);
    cfc->Scf = cfc->Rcf = cfc->LScf = cfc->LRcf = RESET;
}

void
cfc_alog (cfc_t* cfc)
{
    cfc->LScf = cfc->LRcf = SET;
}

int
cfc_amog (cfc_t* cfc)
{
    assert(cfc);
    if (cfc->Scf != cfc->Rcf || cfc->Scf != RESET) {
        return 0;
    } else {
        cfc->Scf = cfc->Rcf = SET;
        return 1;
    }
}

int
cfc_check(cfc_t* cfc)
{
    assert(cfc);
    if (cfc->LScf != cfc->LRcf || cfc->LScf != SET)
        return 0;
    else
        return 1;
}
