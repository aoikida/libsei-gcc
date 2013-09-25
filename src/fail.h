/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_FAIL_H_
#define _ASCO_FAIL_H_
#include <stdio.h>
#include <stdlib.h>

#define ASCO_EXIT exit(EXIT_FAILURE)

#define fail_if(cond, msg)                              \
    if (!(cond)) {                                      \
        fprintf(stderr, "ASCO ERROR: %s\n", msg);       \
        ASCO_EXIT;                                      \
    }

//#define fail_if(cond, msg)

#endif /* _ASCO_FAIL_H_ */
