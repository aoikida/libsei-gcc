/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

#include <asco.h>
#include <stdio.h>
#include <assert.h>

/* MODE = instr|heap|cow|sheap|scow
 * - instr: instrumentation only
 * - heap
 * - cow
 * - sheap:
 * - scow:
 */
#define INSTR_MODE 0
#define HEAP_MODE  1
#define COW_MODE   2
#define SHEAP_MODE 3
#define FCOW_MODE  4

#ifndef MODE
#error MODE should be defined (HEAP_MODE|COW_MODE|INSTR_MODE)
#endif

/* -----------------------------------------------------------------------------
 * algorithm selection
 * -------------------------------------------------------------------------- */

#if MODE == HEAP_MODE
# include "asco-heap_mode.c"
#elif MODE == COW_MODE
# include "asco-cow_mode.c"
#elif MODE == FCOW_MODE
# include "asco-fcow_mode.c"
#elif MODE == INSTR_MODE
#else
# error invalid MODE
#endif
