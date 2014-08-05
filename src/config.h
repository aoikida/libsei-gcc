/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Sergey Arnautov, Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#ifndef _ASCO_CONFIG_H_
#define _ASCO_CONFIG_H_

#define ABUF_MAX_CONFLICTS 2000

#define OBUF_SIZE 10     // at most 10 output messages per traversal
#define COW_SIZE  10000    // at most 100 writes per traversal
#define TBIN_SIZE 10000     // at most 10 frees per traversal
#define TALLOC_MAX_ALLOCS 1000

#ifndef ASCO_MT
/* provide wrappers for system calls */
#define ASCO_WRAP_SC
#endif

#ifdef ASCO_WRAP_SC
#define SC_MAX_CALLS 10 // at most 10 system calls inside a traversal
#endif


#endif /* _ASCO_CONFIG_H_ */
