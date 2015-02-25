/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include <stdlib.h>
#define TMI_IMPL
#include <sei/support.h>


/* ----------------------------------------------------------------------------
 * libc functions that should be transactified
 * ------------------------------------------------------------------------- */

// functions that modify arguments
#include "libc/stdlib/strtol.c"
#include "libc/stdlib/strtoll.c"
#include "libc/stdlib/strtoul.c"
#include "libc/stdlib/strtoull.c"
#include "libc/string/memmove.c"
#include "libc/string/strdup.c"
#include "libc/string/strncpy.c"
#include "libc/string/strcpy.c"
//#include "libc/string/memcpy.c"
//#include "libc/string/memset.c"

// stateless functions
#ifndef COW_ROPURE
# include "libc/string/strlen.c"
# include "libc/string/memchr.c"
# include "libc/string/memcmp.c"
# include "libc/string/strcmp.c"
# include "libc/string/strncmp.c"
# include "libc/string/strchr.c"
#endif

# include "crc.c"

char*
strndup(const char *s, size_t n)
{
    char* ptr = malloc(n+1);
    strncpy(ptr, s, n);
    ptr[n] = '\0';
    return ptr;
}

/* ----------------------------------------------------------------------------
 * special wrappers
 * ------------------------------------------------------------------------- */
/* we define the following wrappers by hand. That refrains the
 * compiler from substituting the untrasactified functions.
 *
 * 1. we rename the function foo() to be wrapped with the suffix, eg,
 * "foo_bsd()".
 *
 * 2. we instrument that function. There will be foo_bsd() and
 * _ZGTt7foo_bsd(), where 7 is the number of characters in the
 * function name.
 *
 * 3. we write _ZGTt3foo() by hand. It should simply call  _ZGTt7foo_bsd().
 *
 */

void* _ZGTt11memmove_bsd(void* dst, const void* src, size_t size);
void*
_ZGTt7memmove(void* dst, const void* src, size_t size)
{
    return _ZGTt11memmove_bsd(dst, src, size);
}

#if 0
int _ZGTt10strcmp_bsd(const char *s1, const char *s2);
int
_ZGTt6strcmp(const char *s1, const char *s2)
{
    return _ZGTt10strcmp_bsd(s1, s2);
}
#endif

#ifdef __APPLE__

int _ZGTt10__maskrune(__darwin_ct_rune_t _c, unsigned long _f)
{
    return 0;
}

int* _ZGTt7__error()
{
    return (int*) malloc(sizeof(int));
}

double _ZGTt4ceil(double x)
{
    return (double) (((long) x) + 1);
}

void* _ZGTt12__memset_chk(void* dst, int c, size_t size, size_t len)
{
    return NULL;
}

void* _ZGTt12__strcpy_chk(void* dst, const void* src, size_t len)
{
    return NULL;
}
#endif /* __APPLE__ */

