/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _ASCO_DEBUG_H_
#define _ASCO_DEBUG_H_
#include <stdio.h>

#if DEBUG >= 1
#define DLOG1(...) printf(__VA_ARGS__)
#else
#define DLOG1(...)
#endif

#if DEBUG >= 2
#define DLOG2(...) printf(__VA_ARGS__)
#else
#define DLOG2(...)
#endif

#if DEBUG >= 3
#define DLOG3(...) printf(__VA_ARGS__)
#else
#define DLOG3(...)
#endif

#endif /* _ASCO_DEBUG_H_ */
