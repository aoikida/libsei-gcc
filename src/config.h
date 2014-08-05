/*
 * config.h
 *
 *  Created on: Aug 5, 2014
 *      Author: sergey
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define ABUF_MAX_CONFLICTS 2000

#define OBUF_SIZE 10     // at most 10 output messages per traversal
#define COW_SIZE  10000    // at most 100 writes per traversal
#define TBIN_SIZE 10000     // at most 10 frees per traversal

#define TALLOC_MAX_ALLOCS 1000

/*sinfo */
#define MAX_ALLOC 20000

#ifndef ASCO_MT
/* provide wrappers for system calls */
#define ASCO_WRAP_SC
#endif

#ifdef ASCO_WRAP_SC
#define MAX_CALLS 10	// at most 10 system calls inside a traversal
#endif


#endif /* CONFIG_H_ */
