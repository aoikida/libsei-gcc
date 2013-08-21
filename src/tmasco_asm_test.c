#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t ret;
} tmasco_ctx_t;
tmasco_ctx_t _ctx;

uint32_t _ITM_beginTransaction(uint32_t flags, ...);
uint32_t tmasco_switch(tmasco_ctx_t* ctx, uint32_t retval);

uint32_t
tmasco_begin(tmasco_ctx_t* ctx)
{
    printf("sizeof %d\n", sizeof(tmasco_ctx_t));
    assert (sizeof(tmasco_ctx_t) == 64);
    memcpy(&_ctx, ctx, sizeof(tmasco_ctx_t));
    return 0x01;
}

int
main(int argc, char* argv[])
{
    uint32_t val = _ITM_beginTransaction(0);
    if (val) {
        printf("first run\n");
        tmasco_switch(&_ctx, 0);
    } else {
        printf("second run\n");
    }
    return 0;
}
