/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#define TMASCO_IMPL
#include <tmasco_support.h>


/* -----------------------------------------------------------------------------
 * libc/string functions that should be transactified
 * -------------------------------------------------------------------------- */

#include "libc/string/strlen.c"
#include "libc/string/strdup.c"
#include "libc/string/memcmp.c"
#include "libc/string/strcmp.c"
#include "libc/string/strncpy.c"
#include "libc/string/strcpy.c"
#include "libc/string/strncmp.c"
#include "libc/string/strchr.c"
#include "libc/string/memset.c"
#include "libc/string/memcpy.c"
#include "libc/string/memmove.c"

char*
strndup(const char *s, size_t n)
{
    char* ptr = malloc(n+1);
    strncpy(ptr, s, n);
    ptr[n] = '\0';
    return ptr;
}

#ifdef USE_CLANG
/* ___string_mock() calls all functions from libc string inside a
 * transaction. This forces the compiler to generate a transactional
 * version from each of these functions.
 *
 * Note that ___string_mock() does invalid calls and should never be
 * called by any real code.
 */

void*
___string_mock() {
    assert (0 && "this function should never be called");

    __transaction_atomic {

        char* str1 = "hello";
        char* str2 = "world";

        // strdup and strndup
        char* x = strdup(str1);
        x = strndup(str1, 2);

        size_t s = strlen(str1);

        // strcpy and strncpy
        x = strcpy(str1, str2);
        x = strncpy(str1, str2, 1);

        // memcpy
        void* y = memcpy(str1, str2, 10);

        // memcmp
        if (memcmp(str1, str2, 2) == 0) {
            ;
        }

        // strncmp
        if (strncmp(str1, str2, 2) == 0) {
            ;
        }
        if (strchr(str1, '\n') == NULL) {
            ;
        }


        // remove 'unused variable' warnings
        y = x;
        y = str1 + s;
        y = str2;
        return y;
    }
}
#endif
