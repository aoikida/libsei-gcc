/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_DEBUG_H_
#define _SEI_DEBUG_H_
#include <stdio.h>

#if DEBUG >= 1
#define DLOG1(...)                                          \
    do {                                                    \
        fprintf(stderr, "[%s:%d](1) ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                       \
    } while (0);
#else
#define DLOG1(...)
#endif

#if DEBUG >= 2
#define DLOG2(...)                                          \
    do {                                                    \
        fprintf(stderr, "[%s:%d](2) ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                       \
    } while (0);
#else
#define DLOG2(...)
#endif

#if DEBUG >= 3
#define DLOG3(...)                                          \
    do {                                                    \
        fprintf(stderr, "[%s:%d](3) ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                       \
    } while (0);

#else
#define DLOG3(...)
#endif

#endif /* _SEI_DEBUG_H_ */
