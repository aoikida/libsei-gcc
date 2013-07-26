/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */

uint32_t _ITM_beginTransaction(uint32_t properties,...) { return 0x01; }
void _ITM_commitTransaction() {}
inline void* _ITM_malloc(size_t size) { return malloc(size); }
inline void _ITM_free(void* ptr) { free(ptr); }
inline void* _ITM_calloc(size_t nmemb, size_t size)
{ return calloc(nmemb, size); }
#define ITM_READ(type,  suffix) inline                  \
    type _ITM_R##suffix(const type* addr)               \
    {                                                   \
        return *addr;                                   \
    }
ITM_READ(uint8_t,  U1)
ITM_READ(uint16_t, U2)
ITM_READ(uint32_t, U4)
ITM_READ(uint64_t, U8)

#define ITM_WRITE(type, suffix) inline                  \
    void _ITM_W##suffix(type* addr, type value)         \
    {                                                   \
        *addr = value;                                  \
    }

ITM_WRITE(uint8_t,  U1)
ITM_WRITE(uint16_t, U2)
ITM_WRITE(uint32_t, U4)
ITM_WRITE(uint64_t, U8)

void
_ITM_WM128(void* txn, __uint128_t* addr, __uint128_t value)
{
    *addr = value;
}

__uint128_t
_ITM_RM128(void* txn, __uint128_t* addr)
{
    return *addr;
}

void*
_ZGTt7realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void _ITM_changeTransactionMode(int flag) {}
void* _ITM_getTMCloneOrIrrevocable(void* ptr) { return ptr;}
void* _ITM_memcpyRtWt(void* dst, const void* src, size_t size)
{ return memcpy(dst, src, size); }

void* _ITM_memmoveRtWt(void* dst, const void* src, size_t size)
{ return memmove(dst, src, size); }

void* _ITM_memsetW(void* s, int c, size_t n)
{ return memset(s,c,n); }

int _ITM_initializeProcess() { return 0; }
