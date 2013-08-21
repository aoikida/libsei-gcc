#include <assert.h>
#include "abuf.h"

void
init_fini()
{
    abuf_t* abuf = abuf_init(100000);
    abuf_fini(abuf);
}

void
push_some()
{
    abuf_t* abuf = abuf_init(100000);

    abuf_push_uint64_t(abuf, (uint64_t*) 0x00, 0xDEADBEEFCAFEBABE);
    abuf_push_uint8_t (abuf, (uint8_t*)  0x00, 0xBB);
    abuf_push_uint64_t(abuf, (uint64_t*) 0x0F, 0xBABEBABEBABEBABE);

    abuf_fini(abuf);
}

void
push_and_pop_some()
{
    abuf_t* abuf = abuf_init(100000);

    abuf_push_uint64_t(abuf, (uint64_t*) 0x00, 0xDEADBEEFCAFEBABE);
    abuf_push_uint8_t (abuf, (uint8_t*)  0x00, 0xBB);
    abuf_push_uint64_t(abuf, (uint64_t*) 0x0F, 0xBABEBABEBABEBABE);

    assert (0xDEADBEEFCAFEBABE == abuf_pop_uint64_t(abuf, (uint64_t*) 0x00));
    assert (0xBB               == abuf_pop_uint8_t (abuf, (uint8_t*) 0x00));
    assert (1                  == abuf_size(abuf));
    assert (0xBABEBABEBABEBABE == abuf_pop_uint64_t(abuf, (uint64_t*) 0x0F));
    assert (0                  == abuf_size(abuf));

    abuf_push_uint64_t(abuf, (uint64_t*) 0xFF, 0xBABEBABEBABEBABE);
    assert (1                  == abuf_size(abuf));
    abuf_clean(abuf);
    assert (0                  == abuf_size(abuf));

    abuf_fini(abuf);
}

int
main(int argc, char* argv[])
{
    init_fini();
    push_some();
    push_and_pop_some();
    return 0;
}
