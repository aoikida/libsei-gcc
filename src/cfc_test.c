/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include "cfc.h"

void
test1()
{
    cfc_t cfc;
    cfc_reset(&cfc);

    // Lcf, L'cf <- SET
    cfc_alog(&cfc);

    // if Scf # Rcf \/ Scf # RESET return 0;
    // Scf1, Rcf <- SET
    // return 1;
    int r = cfc_amog(&cfc);
    assert (r == 1);

    // Lcf # L'cf \/ Lcf # SET return 0;
    // else return 1;
    r = cfc_check(&cfc);
    assert (r == 1);
}

int
main(int argc, char* argv[])
{
    test1();
    return 0;
}
