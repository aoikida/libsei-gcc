/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _CPU_STATS_H_
#define _CPU_STATS_H_

#include <stdint.h>
#include "ilog.h"

typedef struct cpu_stats cpu_stats_t;

cpu_stats_t* cpu_stats_init(void);
void         cpu_stats_fini(cpu_stats_t* stats);
void         cpu_stats_report(cpu_stats_t* stats, ilog_t* ilog);

#endif /* _CPU_STATS_H_ */
