/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include <sei.h>

size_t strlen(const char *s) SEI_PURE;
int    strcmp(const char *s1, const char *s2) SEI_PURE;

#include "ukv.c"
#include "hashtable/hashtable.c"
#include "hashtable/hashtable_utility.c"
#include "hashtable/hashtable_itr.c"
#include "ukv_net.c"
#include "ukv_server.c"
