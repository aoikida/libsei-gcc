/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include "cfc.h"

inline void
cfc_reset(cfc_t* cfc)
{
    assert(cfc);
    cfc->Scf = cfc->Rcf = cfc->LScf = cfc->LRcf = RESET;
}

inline void
cfc_alog (cfc_t* cfc)
{
    assert(cfc);
    cfc->LScf = cfc->LRcf = SET;
}

inline int
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

inline int
cfc_check(cfc_t* cfc)
{
    assert(cfc);
    if (cfc->LScf != cfc->LRcf || cfc->LScf != SET)
        return 0;
    else
        return 1;
}
