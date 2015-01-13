#include <tmasco.h>

size_t strlen(const char *s) TMASCO_PURE;
int strcmp(const char *s1, const char *s2) TMASCO_PURE;

#include "ukv.c"
#include "hashtable/hashtable.c"
#include "hashtable/hashtable_utility.c"
#include "hashtable/hashtable_itr.c"
#include "ukv_net.c"
#include "ukv_server.c"
