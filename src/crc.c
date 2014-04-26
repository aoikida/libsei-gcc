/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include "crc.h"

/* ----------------------------------------------------------------------------
 * implementation
 * ------------------------------------------------------------------------- */
#include "crc32c/crc32c.c"
#include "crc32c/crc32c_hardware.c"
#include "crc32c/crc32c_software.c"

crc32c_f* crc32c;

static void __attribute__((constructor))
crc_module_init()
{
    crc32c = crc32c_impl();
}

/* ----------------------------------------------------------------------------
 * high-level methods
 * ------------------------------------------------------------------------- */

uint32_t
crc_compute(const char* block, size_t len)
{
    return crc_close(
        crc_append_len(
            crc_append(crc_init(), block, len),
            len));
}

/* ----------------------------------------------------------------------------
 * low-level methods
 * ------------------------------------------------------------------------- */

inline uint32_t
crc_init()
{
    return 0xFFFFFFFF;
}

uint32_t
crc_append(uint32_t crc, const char* block, size_t len)
{
#if defined(CRC_CHECKSUM)
    int i = 0;
    for (i = 0; i < len; ++i) crc ^= block[i];
    return crc;
#elif defined(CRC_NONE)
    return 0;
#else // CRC default
    return crc32c(crc, block, len);
#endif

}

uint32_t
crc_append_len(uint32_t crc, size_t len)
{
#if defined(CRC_CHECKSUM)
    return crc ^ len;
#elif defined(CRC_NONE)
    return 0;
#else
    // TODO: add length to CRC
    return crc;
#endif
}

inline uint32_t
crc_close(uint32_t crc)
{
#ifdef CRC_NONE
    return 0;
#else
    return ~crc;
#endif
}

/* ----------------------------------------------------------------------------
 * advanced interface
 * ------------------------------------------------------------------------- */

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
