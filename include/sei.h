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
#define __end()                      } __tmi_end(x)
#define __begin_nm()                 __tmi_prepare_nm(); if (1) { \
                                     __tmi_begin(x) 
#define __output_append(ptr, size)   __tmi_output_append(ptr, size) 
#define __output_done()              __tmi_output_done()
#define __crc_pop()                  __tmi_output_next() 

#endif /* _SEI_H_ */
