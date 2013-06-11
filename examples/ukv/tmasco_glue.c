#include <tmasco.h>

void* ignore_addrs[] = {NULL};

#define TMASCO_ENABLED
#include "ukv.c"
#include "hashtable/hashtable.c"
#include "hashtable/hashtable_utility.c"
#include "hashtable/hashtable_itr.c"
#include "ukv_net.c"
#include "ukv_server.c"
