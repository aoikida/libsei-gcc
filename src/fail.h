/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_FAIL_H_
#define _SEI_FAIL_H_
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(SEI_IGNORE_ERROR)
# define SEI_EXIT
#elif defined(DEBUG)
# define SEI_EXIT assert(0 && "ERROR DETECTED")
#else
# define SEI_EXIT exit(EXIT_FAILURE)
#endif

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define SEI_FAIL(...) do {                                     \
        fprintf(stderr, "ERROR DETECTED (%s:%d) *** ",          \
                __FILE__, __LINE__);                            \
        fprintf(stderr, __VA_ARGS__);                           \
        fprintf(stderr, "\n");                                  \
        SEI_EXIT;                                              \
    } while(0);

#define fail_ifn(cond, msg) if (unlikely(!(cond))) { SEI_FAIL("%s", msg); }

#endif /* _SEI_FAIL_H_ */
