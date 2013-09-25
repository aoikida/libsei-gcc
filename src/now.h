/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _NOW_H_
#define _NOW_H_

#include <sys/time.h>
#include <stdint.h>

/* reads current time in microseconds */
uint64_t inline now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t) tv.tv_sec)*1000000 + (uint64_t)(tv.tv_usec);
}

#define NOW_1S 1000000 // 1 second in microseconds

#endif /* _NOW_H_ */
