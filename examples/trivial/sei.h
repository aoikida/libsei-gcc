#ifndef _SEI_H_
#define _SEI_H_

#include <tmasco.h> // library interface

#define __begin(ptr, size, crc)      __asco_prepare(ptr, size, crc, 0) ) \
                                     {__asco_begin(x) if (1
#define __end(x)                     } __asco_end(x)
#define __begin_nm()                 __asco_prepare_nm() \
                                     __asco_begin(x) 
#define __end_nm(x)                  __asco_end(x)
#define __output_append(ptr, size)   __asco_output_append(ptr, size) 
#define __output_done()              __asco_output_done()
#define __crc_pop()                  __asco_output_next() 

#endif
