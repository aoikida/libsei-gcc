/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <assert.h>
#include "heap.h"

void
test1()
{
    heap_t* heap = heap_init(2048);
    void* p = heap_malloc(heap, 512);
    heap_free(heap, p);
    void* t = heap_malloc(heap, 512);
    assert (t == p);
    heap_free(heap, p);
    heap_fini(heap);
}

int
main()
{
    test1();
    return 0;
}
