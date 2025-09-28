/* Copyright (C) 2002, 2004 Christopher Clark  <firstname.lastname@cl.cam.ac.uk> */

/* Temporarily mask system functions to avoid conflicts with libsei */
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
#define strchr __system_strchr
#define strcmp __system_strcmp
#define strncmp __system_strncmp
#define strlen __system_strlen
#define realloc __system_realloc
#define strndup __system_strndup

#include <stdlib.h> /* defines NULL */

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
#undef strchr
#undef strcmp
#undef strncmp
#undef strlen
#undef realloc
#undef strndup

#include "hashtable.h"
#include "hashtable_private.h"
#include "hashtable_itr.h"

/*****************************************************************************/
/* hashtable_iterator    - iterator constructor */

struct hashtable_itr *
hashtable_iterator(struct hashtable *h)
{
    unsigned int i, tablelength;
    struct hashtable_itr *itr = (struct hashtable_itr *)
        malloc(sizeof(struct hashtable_itr));
    if (NULL == itr) return NULL;
    itr->h = h;
    itr->e = NULL;
    itr->parent = NULL;
    tablelength = h->tablelength;
    itr->index = tablelength;
    if (0 == h->entrycount) return itr;

    for (i = 0; i < tablelength; i++)
    {
        if (NULL != h->table[i])
        {
            itr->e = h->table[i];
            itr->index = i;
            break;
        }
    }
    return itr;
}

/*****************************************************************************/
/* key      - return the key of the (key,value) pair at the current position */
/* value    - return the value of the (key,value) pair at the current position */

void *
hashtable_iterator_key(struct hashtable_itr *i)
{ return i->e->k; }

void *
hashtable_iterator_value(struct hashtable_itr *i)
{ return i->e->v; }

/*****************************************************************************/
/* advance - advance the iterator to the next element
 *           returns zero if advanced to end of table */

int
hashtable_iterator_advance(struct hashtable_itr *itr)
{
    unsigned int j,tablelength;
    struct entry **table;
    struct entry *next;
    if (NULL == itr->e) return 0; /* stupidity check */

    next = itr->e->next;
    if (NULL != next)
    {
        itr->parent = itr->e;
        itr->e = next;
        return -1;
    }
    tablelength = itr->h->tablelength;
    itr->parent = NULL;
    if (tablelength <= (j = ++(itr->index)))
    {
        itr->e = NULL;
        return 0;
    }
    table = itr->h->table;
    while (NULL == (next = table[j]))
    {
        if (++j >= tablelength)
        {
            itr->index = tablelength;
            itr->e = NULL;
            return 0;
        }
    }
    itr->index = j;
    itr->e = next;
    return -1;
}

/*****************************************************************************/
/* remove - remove the entry at the current iterator position
 *          and advance the iterator, if there is a successive
 *          element.
 *          If you want the value, read it before you remove:
 *          beware memory leaks if you don't.
 *          Returns zero if end of iteration. */

int
hashtable_iterator_remove(struct hashtable_itr *itr)
{
    struct entry *remember_e, *remember_parent;
    int ret;

    /* Do the removal */
    if (NULL == (itr->parent))
    {
        /* element is head of a chain */
        itr->h->table[itr->index] = itr->e->next;
    } else {
        /* element is mid-chain */
        itr->parent->next = itr->e->next;
    }
    /* itr->e is now outside the hashtable */
    remember_e = itr->e;
    itr->h->entrycount--;
    freekey(remember_e->k);

    /* Advance the iterator, correcting the parent */
    remember_parent = itr->parent;
    ret = hashtable_iterator_advance(itr);
    if (itr->parent == remember_e) { itr->parent = remember_parent; }
    free(remember_e);
    return ret;
}

/*****************************************************************************/
int /* returns zero if not found */
hashtable_iterator_search(struct hashtable_itr *itr,
                          struct hashtable *h, void *k)
{
    struct entry *e, *parent;
    unsigned int hashvalue, index;

    hashvalue = hash(h,k);
    index = indexFor(h->tablelength,hashvalue);

    e = h->table[index];
    parent = NULL;
    while (NULL != e)
    {
        /* Check hash value to short circuit heavier comparison */
        //XXX if ((hashvalue == e->h) && (h->eqfn(k, e->k)))
        if ((hashvalue == e->h) && (eqfn(k, e->k)))
        {
            itr->index = index;
            itr->e = e;
            itr->parent = parent;
            itr->h = h;
            return -1;
        }
        parent = e;
        e = e->next;
    }
    return 0;
}


/*
 * Copyright (c) 2002, 2004, Christopher Clark
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
