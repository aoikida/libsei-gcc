/* ----------------------------------------------------------------------------
 * Copyright (c) 2014,2015 Sergey Arnautov, Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

#ifndef _SEI_CONFIG_H_
#define _SEI_CONFIG_H_

#define ABUF_MAX_CONFLICTS 8000

#define OBUF_SIZE 10     // at most 10 output messages per traversal
#define COW_SIZE  128    // at most 128 writes per traversal
#define TBIN_SIZE 10000     // at most 10 frees per traversal
#define TALLOC_MAX_ALLOCS 20000

/* abuf and cow data structures are automatically reallocated if their
 * capacity is reached. Uncomment the next lines to disable automatic
 * reallocation. */
//#define ABUF_DISABLE_REALLOC
//#define COW_DISABLE_REALLOC


#ifndef SEI_MT
/* provide wrappers for system calls */
#define SEI_WRAP_SC
#endif

#ifdef SEI_WRAP_SC
#define SC_MAX_CALLS 100 // at most 10 system calls inside a traversal
#endif

#define WTS_MAX_ARG 32	// maximum number of arguments for a wrapped call

#endif /* _SEI_CONFIG_H_ */
