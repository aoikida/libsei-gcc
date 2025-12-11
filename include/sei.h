/* ----------------------------------------------------------------------------
 * Copyright (c) 2013-2015 Diogo Behrens, Sergey Arnautov
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_H_
#define _SEI_H_

#include <sei/tmi.h>

#define __begin(ptr, size, crc)      __tmi_prepare((ptr), (size), (crc), 1) ) \
                                     {__tmi_begin(x) if (1
#define __begin_rw(ptr, size, crc)   __tmi_prepare((ptr), (size), (crc), 0) ) \
                                     {__tmi_begin(x) if (1
#define __begin_n(ptr, size, crc, n) __tmi_prepare_n((ptr), (size), (crc), 1, (n)) ) \
                                     {__tmi_begin(x) if (1
#define __begin_rw_n(ptr, size, crc, n) __tmi_prepare_n((ptr), (size), (crc), 0, (n)) ) \
                                     {__tmi_begin(x) if (1
#define __end()                      } __tmi_end(x)
#define __begin_nm()                 __tmi_prepare_nm(); if (1) { \
                                     __tmi_begin(x)
#define __begin_nm_n(n)              __tmi_prepare_nm_n((n)); if (1) { \
                                     __tmi_begin(x) 
#define __output_append(ptr, size)   __tmi_output_append(ptr, size) 
#define __output_done()              __tmi_output_done()
#define __crc_pop()                  __tmi_output_next() 

#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0


#endif /* _SEI_H_ */
