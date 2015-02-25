/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SUPPORT_H_
#define _SUPPORT_H_
#include <stddef.h>
#include <stdint.h>

#ifndef SEI_ATTR
#define SEI_ATTR __attribute__((transaction_safe))
#ifdef COW_ROPURE
#define SEI_ATTRP __attribute__((transaction_pure))
#endif
#endif

#ifdef TMI_IMPL
#define SEI_PREF
#else
#define SEI_PREF extern
#endif

#define SEI_DECL(RETURN, FUNC, ARGS) SEI_PREF RETURN FUNC ARGS SEI_ATTR;
#ifdef COW_ROPURE
#define SEI_DECLP(RETURN, FUNC, ARGS) SEI_PREF RETURN FUNC ARGS SEI_ATTRP;
#else
#define SEI_DECLP(RETURN, FUNC, ARGS) SEI_PREF RETURN FUNC ARGS SEI_ATTR;
#endif

void __assert_fail (const char *__assertion, const char *__file,
                    unsigned int __line, const char *__function)
    __attribute__((transaction_pure));

SEI_DECLP(size_t, strlen,  (const char*))
SEI_DECLP(int,    strcmp,  (const char*, const char*))
SEI_DECLP(int,    strncmp, (const char*, const char*, size_t))
SEI_DECLP(char*,  strchr,  (const char *s, int c))
SEI_DECLP(void*,  memchr,  (const void *s, int c, size_t n))
SEI_DECLP(int,    memcmp,  (const void*, const void*, size_t size))

SEI_DECL(long,  strtol,  (const char *nptr, char **endptr, int base))
SEI_DECL(long long, strtoll, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long, strtoul, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long long, strtoull,
           (const char *nptr, char **endptr, int base))
SEI_DECL(char*, strdup, (const char *s))
SEI_DECL(char*, strndup, (const char *s, size_t n))
SEI_DECL(char*, strcpy, (char *dest, const char *src))
SEI_DECL(char*, strncpy, (char *dest, const char *src, size_t n))
SEI_DECL(void*, memcpy, (void* dst, const void* src, size_t size))
SEI_DECL(void*, memset, (void *s, int c, size_t n))
SEI_DECL(void*, memmove, (void *dest, const void *src, size_t n))
SEI_DECL(void*, memmove_bsd, (void *dest, const void *src, size_t n))
SEI_DECL(void*, realloc, (void* ptr, size_t size))
    
#ifdef __APPLE__
SEI_DECL(int,    __maskrune, (__darwin_ct_rune_t _c, unsigned long _f))
SEI_DECL(int*,   __error, (void))
SEI_DECL(size_t, __builtin_object_size, (void * ptr, int type))
SEI_DECL(void* , __builtin___memcpy_chk, (void* dst, const void* src,
                                           size_t size, size_t len))
SEI_DECL(void* , __builtin___strcpy_chk, (void* dst, const void* src,
                                           size_t len))
SEI_DECL(void* , __builtin___memset_chk, (void *s, int c, size_t n, size_t x))
SEI_DECL(double, ceil, (double x))
#endif

#ifdef SEI_CLOG
SEI_DECL (uint32_t, crc32cHardware32, (uint32_t crc, const void* block, size_t len))
SEI_DECL (uint32_t, crc32cHardware64, (uint32_t crc, const void* block, size_t len))
SEI_DECL (uint32_t, crc32cSlicingBy8, (uint32_t crc, const void* block, size_t len))
#endif

#endif /* _SUPPORT_H_ */
