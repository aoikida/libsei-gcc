/* ----------------------------------------------------------------------------
 * Copyright (c) 2013,2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#include "asco.h"
#include <stdio.h>
#include <assert.h>

/* MODE = instr|heap|cow
 * - instr: instrumentation only
 * - heap
 * - cow
 */
#define INSTR_MODE 0
#define HEAP_MODE  1
#define COW_MODE   2
#define COR_MODE   3

#ifndef MODE
#error MODE should be defined (HEAP_MODE|COW_MODE|INSTR_MODE)
#endif

/* ----------------------------------------------------------------------------
 * algorithm selection
 * ------------------------------------------------------------------------- */

#if MODE == HEAP_MODE
# include "asco-heap_mode.c"
#elif MODE == COW_MODE
# include "asco-cow_mode.c"
#elif MODE == COR_MODE
# include "asco-cor_mode.c"
#elif MODE == INSTR_MODE
#else
# error invalid MODE
#endif
