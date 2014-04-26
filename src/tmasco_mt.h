/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _TMASCO_MT_H_
#define _TMASCO_MT_H_

#define ASCO_MAX_THREADS 16 // maximum number of threads

#if defined(ASCO_2PL) || defined(ASCO_MTL) || defined(ASCO_MTL2)
# ifndef ASCO_MT
#  define ASCO_MT
# endif
#endif

#if defined(ASCO_MTL2) && !defined(ASCO_MTL)
# define ASCO_MTL
#endif

#if defined(ASCO_2PL) && defined(ASCO_MTL)
# error Cant support 2PL and MTL together
#endif

# include "abuf.h"

/* pthread_mutex methods have to be wrapped. We define some function
 * pointer types to help us. */
#define __USE_GNU // to enable RTLD_DEFAULT
#include <dlfcn.h>
#include <pthread.h>
typedef int (pthread_mutex_lock_f)(pthread_mutex_t* mutex);
typedef int (pthread_mutex_trylock_f)(pthread_mutex_t* mutex);
typedef int (pthread_mutex_unlock_f)(pthread_mutex_t* mutex);

#endif /* _TMASCO_MT_H_ */
