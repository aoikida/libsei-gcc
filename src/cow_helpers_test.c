#include <assert.h>
// test the helpers in HEAP-Mode
#define MODE 1
#include "cow_helpers.h"
#include "cow.h"
#include "heap.h"

typedef struct cow_entry cow_entry_t;
cow_entry_t* cow_find(cow_t* cow, uintptr_t wkey);

void
test_cow_find()
{
    heap_t *heap = heap_init(2048);
    cow_t *cow = cow_init(heap, 100);

    char *x = heap_malloc(heap, 2);

    cow_write_uint8_t(cow, (uint8_t*) &x[0], 0x61);

    assert(heap_rel(heap, &x[0]) != heap_rel(heap, &x[1]));

    // must be both within same cow-block
    assert(cow_find(cow, GETWKEY(heap, &x[0])));
    assert(cow_find(cow, GETWKEY(heap, &x[1])));
}

void
test_key_macros()
{
    heap_t *heap = heap_init(2048);
    cow_t *cow = cow_init(heap, 100);

    char *x = heap_malloc(heap, 2);

    uintptr_t key0 = GETWKEY(heap, &x[0]);
    assert(GETWKEY(heap, GETWADDR(heap, key0)) == key0);

    uintptr_t key1 = GETWKEY(heap, &x[1]);
    assert(GETWKEY(heap, GETWADDR(heap, key1)) == key1);
}

int
main(int argc, char **argv)
{
    if (argc == 1) {
        test_cow_find();
    } else {
        switch (atoi(argv[1])) {
        case 0:
            test_cow_find();
            break;
        case 1:
            test_key_macros();
            break;
        default:
            return 1;
        }
    }
    return 0;
}
