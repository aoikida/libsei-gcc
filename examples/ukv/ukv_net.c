/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

/* ukv_net - parse and execute commands from network
 *
 * set command
 *   request: +key,value\r\n
 *   success: !\r\n
 *   failure: !old value\r\n
 *
 * del command
 *   request: -key\r\n
 *   success: !\r\n
 *
 * get command
 *   request: ?key\r\n
 *   failure: !\r\n
 *   success: !value\r\n
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ukv.h"

const char* __attribute__((transaction_safe))
ukv_recv(ukv_t* ukv, const char* msg)
{
    int cmd = msg[0];
    switch(cmd) {
    case '+': {
        char* k = (char*) msg + 1;
        char* ke = k;
        char* v;
        char* ve;

        while (*ke != ',' && *ke != '\0') ++ke;
        if (*ke == '\0') return strdup("%error\r\n");
        if (ke - k <= 0) return strdup("%error\r\n");

        v = ve = ke+1;
        while (*ve != '\r' && *ve != '\0') ++ve;
        if (*ve == '\0') return strdup("%error\r\n");
        if (ve - v <= 0) return strdup("%error\r\n");

        const char* r = ukv_set(ukv,
                                strndup(k, ke-k),
                                strndup(v, ve-v));
        int rsize = 3;
        if (r) rsize += strlen(r);
        char* s = malloc(rsize+1);
        s[0] = '!';
        if (r) strcpy(s+1, r);
        s[rsize-2] = '\r';
        s[rsize-1] = '\n';
        s[rsize]   = '\0';
        return s;
    }
    case '-': {
        char* k = (char*) msg + 1;
        char* ke = k;

        while (*ke != '\r' && *ke != '\0') ++ke;
        if (*ke == '\0') return strdup("%error\r\n");
        if (ke - k <= 0) return strdup("%error\r\n");

        char* key = strndup(k, ke-k);
        ukv_del(ukv, key);
        free(key);
        return strdup("!\r\n");
    }
    case '?': {
        char* k = (char*) msg + 1;
        char* ke = k;

        while (*ke != '\r' && *ke != '\0') ++ke;
        if (*ke == '\0') return strdup("%error\r\n");
        if (ke - k <= 0) return strdup("%error\r\n");


        char* key = strndup(k, ke-k);
        const char* r = ukv_get(ukv, key);
        free(key);

        int rsize = 3;
        if (r) rsize += strlen(r);
        char* s = malloc(rsize+1);
        s[0] = '!';
        if (r) strcpy(s+1, r);
        s[rsize-2] = '\r';
        s[rsize-1] = '\n';
        s[rsize]   = '\0';
        return s;
    }
    case '%':
        // terminate
        return NULL;

    default:
        return strdup("%error\r\n");
    }
}

void
ukv_done(ukv_t* ukv, const char* reply)
{
    free((void*) reply);
}
