/* ----------------------------------------------------------------------------
 * Copyright (c) 2014 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_MT_H_
#define _SEI_MT_H_

#define SEI_MAX_THREADS 16 // maximum number of threads

#if defined(SEI_2PL) || defined(SEI_MTL) || defined(SEI_MTL2)
# ifndef SEI_MT
#  define SEI_MT
# endif
#endif

#if defined(SEI_MTL2) && !defined(SEI_MTL)
# define SEI_MTL
#endif

#if defined(SEI_2PL) && defined(SEI_MTL)
# error Cant support 2PL and MTL together
#endif

# include "abuf.h"

#if defined(SEI_MT) && defined(SEI_TBAR)
# include "tbar.h"
# include "stash.h"
#endif

/* pthread_mutex methods have to be wrapped. We define some function
 * pointer types to help us. */
#define __USE_GNU // to enable RTLD_DEFAULT
#include <dlfcn.h>
#include <pthread.h>
typedef int (pthread_mutex_lock_f)(pthread_mutex_t* mutex);
typedef int (pthread_mutex_trylock_f)(pthread_mutex_t* mutex);
typedef int (pthread_mutex_unlock_f)(pthread_mutex_t* mutex);

#endif /* _SEI_MT_H_ */
