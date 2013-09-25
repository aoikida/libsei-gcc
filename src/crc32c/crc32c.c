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
#include <stdio.h>
#include <string.h>
#include "crc32c.h"

/* -----------------------------------------------------------------------------
 * algorithm selection
 * -------------------------------------------------------------------------- */

static uint32_t
cpuid(uint32_t input) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
#ifdef __PIC__
    // PIC: Need to save and restore ebx See:
    // http://sam.zoy.org/blog/2007-04-13-shlib-with-non-pic-code-have-inline-assembly-and-pic-mix-well
    asm("pushl %%ebx\n\t" /* save %ebx */
        "cpuid\n\t"
        "movl %%ebx, %[ebx]\n\t" /* save what cpuid just put in %ebx */
        "popl %%ebx" : "=a"(eax), [ebx] "=r"(ebx), "=c"(ecx), "=d"(edx)
        : "a" (input)
        : "cc");
#else
    asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (input));
#endif
    return ecx;
}

crc32c_f*
crc32c_impl() {
    static const int SSE42_BIT = 20;
    uint32_t ecx = cpuid(1);
    int hasSSE42 = ecx & (1 << SSE42_BIT);
    if (hasSSE42) {
#ifdef __LP64__
        return crc32cHardware64;
#else
        return crc32cHardware32;
#endif
    } else {
//return crc32cSlicingBy8;
return NULL;
    }
}
