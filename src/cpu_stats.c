/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#define __USE_GNU   // enable RUSAGE_THREAD
#include <sys/resource.h>
#include <sys/time.h>
#include <inttypes.h>
#include "cpu_stats.h"
#include "now.h"

struct cpu_stats {
    struct {
        double user;
        double sys;
        double total;

        double user_diff;
        double sys_diff;
        double total_diff;
    } self;
    uint64_t last;
};

static inline double
get_time(struct timeval* time)
{
    return (double) time->tv_sec + (double) time->tv_usec / 1000000.0;
}

cpu_stats_t*
cpu_stats_init() {
    struct rusage  usage;

    cpu_stats_t* stats = (cpu_stats_t*) malloc(sizeof(cpu_stats_t));
    assert (stats);

    // measure self CPU time
    if (getrusage(RUSAGE_SELF, &usage) != 0){
        perror("cpu_stats_init rusage");
        exit(EXIT_FAILURE);
    }

    double sys  = get_time(&usage.ru_stime);
    double user = get_time(&usage.ru_utime);

    stats->self.sys   = sys;
    stats->self.user  = user;
    stats->self.total = sys + user;
    stats->self.sys_diff   = 0;
    stats->self.user_diff  = 0;
    stats->self.total_diff = 0;

    stats->last = now();

    return stats;
}

void
cpu_stats_fini(cpu_stats_t* stats)
{
    assert (stats);
    free(stats);
}

static void
cpu_measure(cpu_stats_t* stats) {
    struct rusage usage;

    // measure self CPU time
    if (getrusage(RUSAGE_SELF, &usage) != 0){
        perror("cpu_stats_init rusage");
        exit(EXIT_FAILURE);
    }

    double sys   = get_time(&usage.ru_stime);
    double user  = get_time(&usage.ru_utime);
    double total = sys + user;

    stats->self.sys_diff   = sys   - stats->self.sys;
    stats->self.user_diff  = user  - stats->self.user;
    stats->self.total_diff = total - stats->self.total;

    stats->self.sys   = sys;
    stats->self.user  = user;
    stats->self.total = total;

    // make it relative to the elapsed time
    uint64_t ts = now();

    double elapsed = (ts - stats->last)*1.0 / NOW_1S;
    stats->last = ts;

    stats->self.sys_diff   /= elapsed;
    stats->self.user_diff  /= elapsed;
    stats->self.total_diff /= elapsed;
}

void
cpu_stats_report(cpu_stats_t* stats, ilog_t* ilog) {
    char buffer[128];

    cpu_measure(stats);
    sprintf(buffer, "%f %f %f", stats->self.sys_diff, stats->self.user_diff,
            stats->self.total_diff);
    ilog_push(ilog, "cpu", buffer);
}
