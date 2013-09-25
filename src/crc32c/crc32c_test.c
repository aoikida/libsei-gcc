/*-----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "crc32c.h"

#define FUNC(foo) do {                          \
        crc = foo(init, msg, strlen(msg));      \
        printf("%20s: 0x%X\n", #foo, crc);      \
    } while (0)

int
main(int argc, char* argv[])
{
    uint32_t init;
    uint32_t crc;

    if (argc == 2) {
        char* msg = argv[1];

        init = crc32c_init();
        FUNC(crc32cSarwate);
        FUNC(crc32cSlicingBy4);
        FUNC(crc32cSlicingBy8);
        FUNC(crc32cHardware32);
        FUNC(crc32cHardware64);
        printf("--------------------\n");

        init = 0;
        FUNC(crc32cSarwate);
        FUNC(crc32cSlicingBy4);
        FUNC(crc32cSlicingBy8);
        FUNC(crc32cHardware32);
        FUNC(crc32cHardware64);
        printf("--------------------\n");

        return 0;
    }
    init = crc32c_init();
    char* abc = "aaaabbbcccc";
    char* a = "aaaa";
    char* b = "bbb";
    char* c = "cccc";

    uint32_t r1 = crc32cSarwate(init, a, strlen(a));
    uint32_t r2 = crc32cSarwate(r1, b, strlen(b));
    uint32_t r3 = crc32cSarwate(r2, c, strlen(c));
    uint32_t r  = crc32cSarwate(init, abc, strlen(abc));
    printf("0x%x == 0x%x\n", r3, r);
    printf("--------------------\n");

    char* abc2 = "aaaaxxxcccc";
    char* b2 = "xxx";
    printf("sizeof =  %lu\n", strlen(c) * 8);
    uint32_t r4 = crc32cSarwate(init, b2, strlen(b2));
    uint32_t rx = crc32cSarwate(init, b, strlen(b));
    r4 = r4 ^ rx;
    uint32_t r5 = __builtin_ia32_crc32si(0, r4);
    r3 ^= r5;

    uint32_t r6 = crc32cSarwate(init, abc2, strlen(abc2));
    printf("0x%x == 0x%x\n", r3, r6);
    printf("--------------------\n");

    char* abc4 = "aaaabbbbcccc";
    char* abc3 = "xxxxbbbbcccc";
    char* a2 = "xxxx";
    uint32_t r7 = crc32cSarwate(init, abc3, strlen(abc3));
    r6 = crc32cSarwate(init, abc4, strlen(abc4));

    r4 = crc32cSarwate(init, a2, strlen(a2));
    rx = crc32cSarwate(init, a, strlen(a));
    r4 = r4 ^ rx;
    r5 = __builtin_ia32_crc32si(0, r4);
    r5 = __builtin_ia32_crc32si(0, r5);

    r3 = r6 ^ r5;


    printf("0x%x == 0x%x\n", r3, r7);
    printf("--------------------\n");




    return 0;
}
