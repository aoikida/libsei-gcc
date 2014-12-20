/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _TMASCO_SUPPORT_H_
#define _TMASCO_SUPPORT_H_
#include <stddef.h>

#ifdef __APPLE__
#include <ctype.h>
#endif

#ifndef ASCO_ATTR
#define ASCO_ATTR __attribute__((transaction_safe))
#ifdef COW_ROPURE
#define ASCO_ATTRP __attribute__((transaction_pure))
#endif
#endif

#ifdef TMASCO_IMPL
#define ASCO_PREF
#else
#define ASCO_PREF extern
#endif

#define ASCO_DECL(RETURN, FUNC, ARGS) ASCO_PREF RETURN FUNC ARGS ASCO_ATTR;
#ifdef COW_ROPURE
#define ASCO_DECLP(RETURN, FUNC, ARGS) ASCO_PREF RETURN FUNC ARGS ASCO_ATTRP;
#else
#define ASCO_DECLP(RETURN, FUNC, ARGS) ASCO_PREF RETURN FUNC ARGS ASCO_ATTR;
#endif

void __assert_fail (const char *__assertion, const char *__file,
                    unsigned int __line, const char *__function)
    __attribute__((transaction_pure));

#ifdef __APPLE__
ASCO_DECLP(int, __maskrune, (__darwin_ct_rune_t _c, unsigned long _f))
ASCO_DECLP(int*, __error, (void))
ASCO_DECLP(size_t, __builtin_object_size, (void * ptr, int type))
ASCO_DECLP(void* , __builtin___memcpy_chk, (void* dst, const void* src, size_t size, size_t len))
ASCO_DECLP(void* , __builtin___strcpy_chk, (void* dst, const void* src, size_t len))
ASCO_DECLP(void* , __builtin___memset_chk, (void *s, int c, size_t n, size_t len))
#endif 

ASCO_DECLP(size_t, strlen,  (const char*))
ASCO_DECLP(int,    strcmp,  (const char*, const char*))
ASCO_DECLP(int,    strncmp, (const char*, const char*, size_t))
ASCO_DECLP(char*,  strchr,  (const char *s, int c))
ASCO_DECLP(void*,  memchr,  (const void *s, int c, size_t n))
ASCO_DECLP(int,    memcmp,  (const void*, const void*, size_t size))

ASCO_DECL(long,  strtol,  (const char *nptr, char **endptr, int base))
ASCO_DECL(long long, strtoll, (const char *nptr, char **endptr, int base))
ASCO_DECL(unsigned long, strtoul, (const char *nptr, char **endptr, int base))
ASCO_DECL(unsigned long long, strtoull,
           (const char *nptr, char **endptr, int base))
ASCO_DECL(char*, strdup, (const char *s))
ASCO_DECL(char*, strndup, (const char *s, size_t n))
ASCO_DECL(char*, strcpy, (char *dest, const char *src))
ASCO_DECL(char*, strncpy, (char *dest, const char *src, size_t n))
ASCO_DECL(void*, memcpy, (void* dst, const void* src, size_t size))
ASCO_DECL(void*, memset, (void *s, int c, size_t n))
ASCO_DECL(void*, memmove, (void *dest, const void *src, size_t n))
ASCO_DECL(void*, memmove_bsd, (void *dest, const void *src, size_t n))
ASCO_DECL(void*, realloc, (void* ptr, size_t size))

#endif /* _TMASCO_SUPPORT_H_ */
