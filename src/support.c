/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>

/* Temporarily disable system function declarations */
#define strtol __system_strtol
#define strtoll __system_strtoll
#define strtoul __system_strtoul
#define strtoull __system_strtoull
#define strdup __system_strdup
#define strcpy __system_strcpy
#define strncpy __system_strncpy
#define memmove __system_memmove
#define memcpy __system_memcpy
#define memset __system_memset
#define realloc __system_realloc
#define strndup __system_strndup

/* Include system headers with renamed functions */
#include <stdlib.h>
#include <string.h>

/* Restore original function names */
#undef strtol
#undef strtoll
#undef strtoul
#undef strtoull
#undef strdup
#undef strcpy
#undef strncpy
#undef memmove
#undef memcpy
#undef memset
#undef realloc
#undef strndup

/* Now include libsei declarations */
#define TMI_IMPL
#include <sei/support.h>

#ifdef __APPLE__
#include <math.h>
#include <errno.h>
#endif


/* ----------------------------------------------------------------------------
 * Transaction-safe libsei implementations
 * GCC TM will automatically choose these when in transaction context
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
// memcpy and memset handled by ITM interface
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

__attribute__((transaction_safe)) char*
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
    return __maskrune(_c, _f);
}

int* _ZGTt7__error()
{
    return __error();
}

double _ZGTt4ceil(double x)
{
    return ceil(x);
}

void* _ZGTt12__memset_chk(void* dst, int c, size_t size, size_t len)
{
    return __memset_chk(dst, c, size, len);
}

void* _ZGTt12__strcpy_chk(void* dst, const void* src, size_t len)
{
    return __strcpy_chk(dst, src, len);
}
#endif /* __APPLE__ */

