/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _TMASCO_SUPPORT_H_
#define _TMASCO_SUPPORT_H_
#include <stddef.h>

#ifndef ASCO_ATTR
#define ASCO_ATTR __attribute__((transaction_safe))
#endif

#ifdef TMASCO_IMPL
#define ASCO_PREF
#else
#define ASCO_PREF extern
#endif

#define ASCO_DECL(RETURN, FUNC, ARGS) ASCO_PREF RETURN FUNC ARGS ASCO_ATTR;

ASCO_DECL(char*, strdup, (const char *s))
ASCO_DECL(char*, strndup, (const char *s, size_t n))
ASCO_DECL(void*, memcpy, (void* dst, const void* src, size_t size))
ASCO_DECL(size_t, strlen, (const char*))
ASCO_DECL(int, memcmp, (const void*, const void*, size_t size))
ASCO_DECL(int, strcmp, (const char*, const char*))
ASCO_DECL(int, strncmp, (const char*, const char*, size_t))
ASCO_DECL(char*, strcpy, (char *dest, const char *src))
ASCO_DECL(char*, strncpy, (char *dest, const char *src, size_t n))
ASCO_DECL(char*, strdup, (const char *s))
ASCO_DECL(char*, strndup, (const char *s, size_t n))

#endif /* _TMASCO_SUPPORT_H_ */
