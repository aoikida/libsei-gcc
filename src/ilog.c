/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h> // gettid()
#include <unistd.h>
#include <sys/syscall.h>
#include "ilog.h"
#include "now.h"

/* ----------------------------------------------------------------------------
 * types and data structures
 * ------------------------------------------------------------------------- */

struct ilog {
    uint64_t ts;
    FILE* fp;
};

/* ----------------------------------------------------------------------------
 * interface functions
 * ------------------------------------------------------------------------- */

ilog_t*
ilog_init(const char* fname)
{
    ilog_t* ilog = (ilog_t*) malloc(sizeof(ilog_t));
    assert (ilog);
    ilog->ts = now();
    printf("ilog file: %s\n", fname);
    ilog->fp = fopen(fname, "w+");
    assert (ilog->fp);
    return ilog;
}


void
ilog_fini(ilog_t* ilog)
{
    fflush(ilog->fp);
    fclose(ilog->fp);
    free (ilog);
}

void
ilog_push(ilog_t* ilog, const char* topic, const char* info)
{
    uint64_t ts = now();
    //fprintf(ilog->fp, "%lu:%lu:%ld:%s:%s\n", (long unsigned int) ts, (long unsigned int) (ts - ilog->ts), (long unsigned int) syscall(SYS_gettid), topic, info);
    fflush(ilog->fp);
}
