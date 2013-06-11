/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#include "ukv_net.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ukv.h"

void
test_net_set()
{
    ukv_t* ukv = ukv_init();

    const char* r = ukv_recv(ukv, "+key,value\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "+key,value\r\n");
    assert (strcmp(r, "!value\r\n") == 0);
    ukv_done(ukv, r);

    ukv_fini(ukv);
}


void
test_net_get()
{
    ukv_t* ukv = ukv_init();

    const char* r = ukv_recv(ukv, "?key\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "+key,value\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "?key\r\n");
    assert (strcmp(r, "!value\r\n") == 0);
    ukv_done(ukv, r);

    ukv_fini(ukv);
}


void
test_net_del()
{
    ukv_t* ukv = ukv_init();

    const char* r = ukv_recv(ukv, "?key\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "+key,value\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "?key\r\n");
    assert (strcmp(r, "!value\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "-key\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    r = ukv_recv(ukv, "?key\r\n");
    assert (strcmp(r, "!\r\n") == 0);
    ukv_done(ukv, r);

    ukv_fini(ukv);
}

int
main(const int argc, const char* argv[])
{
    test_net_set();
    test_net_get();
    test_net_del();
    return 0;
}
