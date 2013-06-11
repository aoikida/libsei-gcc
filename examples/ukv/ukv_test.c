/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include <assert.h>
#include <string.h>
#include "ukv.h"

void
test_init()
{
    ukv_t* ukv = ukv_init();
    ukv_fini(ukv);
}

void
test_set()
{
    ukv_t* ukv = ukv_init();

    const char* key = "key";
    const char* value = "value";

    const char* r = ukv_set(ukv, strdup(key), strdup(value));
    assert (r == NULL);

    r = ukv_set(ukv, strdup(key), strdup(value));
    assert (r != NULL);

    ukv_fini(ukv);
}

void
test_get()
{
    ukv_t* ukv = ukv_init();

    const char* key = "key";
    const char* value = "value";

    const char* r = ukv_get(ukv, key);
    assert (r == NULL);

    r = ukv_set(ukv, strdup(key), strdup(value));
    assert (r == NULL);

    r = ukv_get(ukv, key);
    assert (r != NULL && strcmp(value, r) == 0);

    ukv_fini(ukv);
}

void
test_del()
{
    ukv_t* ukv = ukv_init();

    const char* key = "key";
    const char* value = "value";

    const char* r = ukv_get(ukv, key);
    assert (r == NULL);

    r = ukv_set(ukv, strdup(key), strdup(value));
    assert (r == NULL);

    r = ukv_get(ukv, key);
    assert (r != NULL && strcmp(value, r) == 0);

    ukv_del(ukv, key);

    r = ukv_get(ukv, key);
    assert (r == NULL);

    ukv_fini(ukv);
}

int
main(const int argc, const char* argv[])
{
    test_init();
    test_set();
    test_get();
    test_del();

    return 0;
}
