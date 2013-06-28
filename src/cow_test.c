#include <cow.h>
#include <assert.h>

typedef struct {
    uint64_t value0;
    uint64_t value1;
    uint32_t value2;
    uint32_t value3;
    uint8_t  value4;
    uint16_t value5;
} aligned_t;

void
test_align()
{
    cow_t* cow = cow_init(100);
    aligned_t x;

    cow_write_uint64_t(cow, &x.value0, 0xDEADBEEFDEADBEEF);
    cow_write_uint64_t(cow, &x.value1, 0xBEEFBEEFBAC0B1C0);
    cow_write_uint32_t(cow, &x.value2, 0xDEADBAC0);
    cow_write_uint32_t(cow, &x.value3, 0xBBBBAAAA);
    cow_write_uint8_t (cow, &x.value4, 0xCC);
    cow_write_uint16_t(cow, &x.value5, 0xDDDD);

    cow_apply(cow);

    assert (x.value0 == 0xDEADBEEFDEADBEEF);
    assert (x.value1 == 0xBEEFBEEFBAC0B1C0);
    assert (x.value2 == 0xDEADBAC0);
    assert (x.value3 == 0xBBBBAAAA);
    assert (x.value4 == 0xCC);
    assert (x.value5 == 0xDDDD);

    cow_fini(cow);
}


int
main(int argc, char* argv[])
{
    test_align();

    return 0;
}
