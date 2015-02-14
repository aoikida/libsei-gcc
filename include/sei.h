/* ----------------------------------------------------------------------------
 * Copyright (c) 2013-2015 Diogo Behrens, Sergey Arnautov
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_H_
#define _SEI_H_

#include <support/tmasco.h> 

#define __begin(ptr, size, crc)      __asco_prepare(ptr, size, crc, 0) ) \
                                     {__asco_begin(x) if (1
#define __begin_rw(ptr, size, crc)   __asco_prepare(ptr, size, crc, 1) ) \
                                     {__asco_begin(x) if (1
#define __end()                      } __asco_end(x)
#define __begin_nm()                 __asco_prepare_nm() \
                                     __asco_begin(x) 
#define __end_nm()                   __asco_end(x)
#define __output_append(ptr, size)   __asco_output_append(ptr, size) 
#define __output_done()              __asco_output_done()
#define __crc_pop()                  __asco_output_next() 

#endif
