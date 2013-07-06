/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* -----------------------------------------------------------------------------
 * types and data structures
 * -------------------------------------------------------------------------- */

#include "sinfo.h"

struct sinfo {
    void*  addr;
    size_t size;
    char** strace;
    size_t ssize;
};

/* -----------------------------------------------------------------------------
 * constructor/destructor and interface methods
 * -------------------------------------------------------------------------- */

sinfo_t*
sinfo_init(void* addr)
{
    sinfo_t* sinfo = (sinfo_t*) malloc(sizeof(sinfo_t));
    assert (sinfo && "out of memory");

    sinfo->addr = addr;
    void* frames[MAX_DEPTH];
    sinfo->ssize  = backtrace(frames, MAX_DEPTH);
    sinfo->strace = backtrace_symbols(frames, sinfo->ssize);

    return sinfo;
}

void
sinfo_fini(sinfo_t* sinfo)
{
    assert (sinfo);
    free(sinfo->strace);
    free(sinfo);
}

void
sinfo_update(sinfo_t* sinfo, void* addr)
{
    assert (sinfo);

    sinfo->addr = addr;
    void* frames[MAX_DEPTH];
    sinfo->ssize  = backtrace(frames, MAX_DEPTH);

    assert (sinfo->strace);
    free(sinfo->strace);
    sinfo->strace = backtrace_symbols(frames, sinfo->ssize);
}

void
sinfo_show(sinfo_t* sinfo)
{
    printf("Stack Info: addr = %p \n", sinfo->addr);
    int i;
    // use an offset of 2 to remove malloc_info and wrapper frames
    for (i = 2; i < sinfo->ssize; ++i) {
#ifdef ASCO_STACK_INFO_CMD
#define STR_(x) #x
#define STR(x) STR_(x)
        char cmd[1024];
        char* p1 = strchr(sinfo->strace[i], '[');
        char* p2 = strchr(sinfo->strace[i], ']');
        char addr[24];
        strncpy(addr, p1+1, p2-p1-1);
        addr[p2-p1-1] = '\0';
        sprintf(cmd, "addr2line -e %s %s", STR(ASCO_STACK_INFO_CMD), addr);
        system(cmd);
#else
        printf("\t%s\n", sinfo->strace[i]);
#endif
    }
}
