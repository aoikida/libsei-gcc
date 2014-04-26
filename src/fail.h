/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ASCO_FAIL_H_
#define _ASCO_FAIL_H_
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(ASCO_IGNORE_ERROR)
# define ASCO_EXIT
#elif defined(DEBUG)
# define ASCO_EXIT assert(0 && "ERROR DETECTED")
#else
# define ASCO_EXIT exit(EXIT_FAILURE)
#endif

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define ASCO_FAIL(...) do {                                     \
        fprintf(stderr, "ERROR DETECTED (%s:%d) *** ",          \
                __FILE__, __LINE__);                            \
        fprintf(stderr, __VA_ARGS__);                           \
        fprintf(stderr, "\n");                                  \
        ASCO_EXIT;                                              \
    } while(0);

#define fail_ifn(cond, msg) if (unlikely(!(cond))) { ASCO_FAIL("%s", msg); }

#endif /* _ASCO_FAIL_H_ */
