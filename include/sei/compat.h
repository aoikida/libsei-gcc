#ifndef SEI_COMPAT_H
#define SEI_COMPAT_H

/* Disable fortified functions first */
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0

/* Mask system functions */
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
#define memcmp __system_memcmp
#define memchr __system_memchr
#define strcmp __system_strcmp
#define strncmp __system_strncmp
#define strchr __system_strchr
#define strlen __system_strlen
#define realloc __system_realloc
#define strndup __system_strndup

#include <stdio.h>
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
#undef memcmp
#undef memchr
#undef strcmp
#undef strncmp
#undef strchr
#undef strlen
#undef realloc
#undef strndup


#endif /* SEI_COMPAT_H */