/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */

uint32_t _ITM_beginTransaction(uint32_t properties,...) { return 0x01; }
void _ITM_commitTransaction() {}
inline void* _ITM_malloc(size_t size) { return malloc(size); }
inline void _ITM_free(void* ptr) { free(ptr); }
inline void* _ITM_calloc(size_t nmemb, size_t size)
{ return calloc(nmemb, size); }

#ifndef COW_ASMREAD
#define ITM_READ(type, prefix, suffix) inline           \
    type _ITM_R##prefix##suffix(const type* addr)       \
    {                                                   \
        return *addr;                                   \
    }

#define ITM_READ_ALL(type, suffix)                      \
    ITM_READ(type,   , suffix)                          \
    ITM_READ(type, aR, suffix)                          \
    ITM_READ(type, aW, suffix)                          \
    ITM_READ(type, fW, suffix)

ITM_READ_ALL(uint8_t,  U1)
ITM_READ_ALL(uint16_t, U2)
ITM_READ_ALL(uint32_t, U4)
ITM_READ_ALL(uint64_t, U8)
#endif


#define ITM_WRITE(type, prefix, suffix) inline                  \
    void _ITM_W##prefix##suffix(type* addr, type value)         \
    {                                                           \
        *addr = value;                                          \
    }

#define ITM_WRITE_ALL(type, suffix)                            \
    ITM_WRITE(type,   , suffix)                                \
    ITM_WRITE(type, aR, suffix)                                \
    ITM_WRITE(type, aW, suffix)

ITM_WRITE_ALL(uint8_t,  U1)
ITM_WRITE_ALL(uint16_t, U2)
ITM_WRITE_ALL(uint32_t, U4)
ITM_WRITE_ALL(uint64_t, U8)
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

void*
_ZGTt6memcpy(void* dst, const void* src, size_t size)
{
    return _ITM_memcpyRtWt(dst, src, size);
}

void*
_ZGTt6memset(void* s, int c, size_t n)
{
    return _ITM_memsetW(s,c,n);
}
