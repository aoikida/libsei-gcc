/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include "crc.h"

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */



/* -----------------------------------------------------------------------------
 * basic methods
 * -------------------------------------------------------------------------- */

uint32_t
crc_compute(char* block, size_t len)
{
    return crc_close(
        crc_append_len(
            crc_append(crc_init(), block, len),
            len));
}

/* -----------------------------------------------------------------------------
 * detailed methods
 * -------------------------------------------------------------------------- */

uint32_t
crc_init()
{
    return 0xFFFFFFFF;
}

uint32_t
crc_append(uint32_t crc, const char* block, size_t len)
{
    int i = 0;
    for (i = 0; i < len; ++i) crc ^= block[i];
    return crc;
}

uint32_t
crc_append_len(uint32_t crc, size_t len)
{
    return crc ^ len;
}

uint32_t
crc_close(uint32_t crc)
{
    return ~crc;
}

/* -----------------------------------------------------------------------------
 * advanced methods
 * -------------------------------------------------------------------------- */

uint32_t
crc_word(uint64_t word)
{
    return 0;
}

uint32_t
crc_delta(uint64_t diff, size_t roff)
{
    return 0;
}
