/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */


#include <inttypes.h>
#include <bits/wordsize.h>
#if __WORDSIZE != 64 && __WORDSIZE != 32
#error support only 32 or 64 bits
#endif
#define HASH(k) (k % COW_MAX)

#if __WORDSIZE == 64
#define WORD_SHIFT           3
#else
#define WORD_SHIFT           2
#endif

#define ADDR2KEY(addr)        (((uintptr_t) addr) >> WORD_SHIFT)
#define KEY2ADDR(addr)        (((uintptr_t) addr) << WORD_SHIFT)

#if MODE == 1 // HEAP_MODE only
#define GETWKEY(heap, addr)   ((uintptr_t) ADDR2KEY(heap_rel(heap, addr)))
#define GETWADDR(heap, wkey)  ((uintptr_t) heap_get(heap, (size_t) KEY2ADDR(wkey)))
#else
#define GETWKEY(heap, addr)   ADDR2KEY(addr)
#define GETWADDR(heap, wkey)  KEY2ADDR(wkey)
#endif


#if __WORDSIZE == 64
typedef uint64_t addr_t;
#else
typedef uint32_t addr_t;
#endif

#define TYPEMASK(addr, type)  ((uintptr_t) addr & (sizeof(type) - 1))
#if __WORDSIZE == 64
#define PICKMASK(addr, type)  (((uintptr_t) addr & 0x07) >> (sizeof(type) >> 1))
#else
#define PICKMASK(addr, type)  (((uintptr_t) addr & 0x03) >> (sizeof(type) >> 1))
#endif

#if __WORDSIZE == 64
#define WVAL(e) (e->wvalue._uint64_t.value[0])
#else
#define WVAL(e) (e->wvalue._uint32_t.value[0])
#endif

#ifndef COW_WT
#define WVAX(e, type, addr) (e->wvalue._##type.value[PICKMASK(addr,type)])
#else
#define WVAX(e, type, addr) (*addr)
#endif
