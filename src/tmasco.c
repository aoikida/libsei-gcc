/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <asco.h>
#include <assert.h>

#define SIZE_U1 sizeof(uint8_t)
#define SIZE_U2 sizeof(uint16_t)
#define SIZE_U4 sizeof(uint32_t)
#define SIZE_U8 sizeof(uint64_t)

typedef uint8_t     _ITM_TYPE_U1;
typedef uint16_t    _ITM_TYPE_U2;
typedef uint32_t    _ITM_TYPE_U4;
typedef uint64_t    _ITM_TYPE_U8;
typedef float       _ITM_TYPE_F;
typedef double      _ITM_TYPE_D;
typedef long double _ITM_TYPE_E;
typedef float _Complex _ITM_TYPE_CF;
typedef double _Complex _ITM_TYPE_CD;
typedef long double _Complex _ITM_TYPE_CE;

extern asco_t* asco;

uint32_t
_ITM_beginTransaction(uint32_t properties,...)
{
    /* Return values to run
     *  - uninstrumented code : 0x02
     *  - instrumented code   : 0x01
     */
    return 0x01;
}

void _ITM_commitTransaction() {}


void*
_ITM_malloc(size_t size)
{
    return asco_malloc(asco, size);
}

void
_ITM_free(void* ptr)
{
    asco_free(asco, ptr);
}

void*
_ITM_calloc(size_t nmemb, size_t size)
{
    assert (0 && "not implemented");
}

#define ITM_READ(type,  suffix)                 \
    type _ITM_R##suffix(const type* addr)       \
    { return asco_read_##type(asco, addr); }

ITM_READ(uint8_t,  U1)
ITM_READ(uint16_t, U2)
ITM_READ(uint32_t, U4)
ITM_READ(uint64_t, U8)

#define ITM_WRITE(type, suffix)                 \
    void _ITM_W##suffix(type* addr, type value) \
    { asco_write_##type(asco, addr, value); }

ITM_WRITE(uint8_t,  U1)
ITM_WRITE(uint16_t, U2)
ITM_WRITE(uint32_t, U4)
ITM_WRITE(uint64_t, U8)

/* methods used by clang-tm */
int _ITM_initializeProcess() { return 0; }
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr) {}
