/*-----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Copyright (c) 2008,2009,2010 Massachusetts Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of the Massachusetts Institute of Technology nor
 *   the names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * -------------------------------------------------------------------------- */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// Hardware-accelerated CRC-32C (using CRC32 instruction)
uint32_t 
crc32cHardware32(uint32_t crc, const void* data, size_t length)
{
    const char* p_buf = (const char*) data;
    // alignment doesn't seem to help?
    size_t i;
    for (i = 0; i < length / sizeof(uint32_t); i++) {
        crc = __builtin_ia32_crc32si(crc, *(uint32_t*) p_buf);
        p_buf += sizeof(uint32_t);
    }

    // This ugly switch is slightly faster for short strings than the
    // straightforward loop
    length &= sizeof(uint32_t) - 1;
    /*
    while (length > 0) {
        crc32bit = __builtin_ia32_crc32qi(crc32bit, *p_buf++);
        length--;
    }
    */
    switch (length) {
    case 3:
        crc = __builtin_ia32_crc32qi(crc, *p_buf++);
    case 2:
        crc = __builtin_ia32_crc32hi(crc, *(uint16_t*) p_buf);
        break;
    case 1:
        crc = __builtin_ia32_crc32qi(crc, *p_buf);
        break;
    case 0:
        break;
    default:
        // This should never happen; enable in debug code
        assert(0);
    }

    return crc;
}

// Hardware-accelerated CRC-32C (using CRC32 instruction)
uint32_t
crc32cHardware64(uint32_t crc, const void* data, size_t length)
{
#ifndef __LP64__
    return crc32cHardware32(crc, data, length);
#else
    const char* p_buf = (const char*) data;
    // alignment doesn't seem to help?
    uint64_t crc64bit = crc;
    size_t i;
    for (i = 0; i < length / sizeof(uint64_t); i++) {
        crc64bit = __builtin_ia32_crc32di(crc64bit, *(uint64_t*) p_buf);
        p_buf += sizeof(uint64_t);
    }

    // This ugly switch is slightly faster for short strings than the
    // straightforward loop
    uint32_t crc32bit = (uint32_t) crc64bit;
    length &= sizeof(uint64_t) - 1;
    /*
    while (length > 0) {
        crc32bit = __builtin_ia32_crc32qi(crc32bit, *p_buf++);
        length--;
    }
    */
    switch (length) {
    case 7:
        crc32bit = __builtin_ia32_crc32qi(crc32bit, *p_buf++);
    case 6:
        crc32bit = __builtin_ia32_crc32hi(crc32bit, *(uint16_t*) p_buf);
        p_buf += 2;
        // case 5 is below: 4 + 1
    case 4:
        crc32bit = __builtin_ia32_crc32si(crc32bit, *(uint32_t*) p_buf);
        break;
    case 3:
        crc32bit = __builtin_ia32_crc32qi(crc32bit, *p_buf++);
    case 2:
        crc32bit = __builtin_ia32_crc32hi(crc32bit, *(uint16_t*) p_buf);
        break;
    case 5:
        crc32bit = __builtin_ia32_crc32si(crc32bit, *(uint32_t*) p_buf);
        p_buf += 4;
    case 1:
        crc32bit = __builtin_ia32_crc32qi(crc32bit, *p_buf);
        break;
    case 0:
        break;
    default:
        // This should never happen; enable in debug code
        assert(0);
    }

    return crc32bit;
#endif
}
