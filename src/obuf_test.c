#include <string.h>
#include <assert.h>
#include "obuf.h"
#include "crc.h"

void
init_fini()
{
    obuf_t* obuf = obuf_init(10);
    obuf_fini(obuf);
}

void
one_message()
{
    obuf_t* obuf = obuf_init(10);
    char* msg = "hello world";

    // first execution
    obuf_push(obuf, msg, strlen(msg));
    obuf_done(obuf);
    obuf_close(obuf);

    // second execution
    obuf_push(obuf, msg, strlen(msg));
    obuf_done(obuf);
    obuf_close(obuf);

    // pop checks
    uint32_t crc = obuf_pop(obuf);
    assert (crc == crc_compute(msg, strlen(msg)));

    // check empty
    assert (obuf_size(obuf) == 0);

    // clean up
    obuf_fini(obuf);
}


void
two_part_message()
{
    obuf_t* obuf = obuf_init(10);
    char* msg = "hello world"; // complete message
    char* msg1 = "hello ";
    char* msg2 = "world";

    // first execution
    obuf_push(obuf, msg1, strlen(msg1));
    obuf_push(obuf, msg2, strlen(msg2));
    obuf_done(obuf);
    obuf_close(obuf);

    // second execution
    obuf_push(obuf, msg1, strlen(msg1));
    obuf_push(obuf, msg2, strlen(msg2));
    obuf_done(obuf);
    obuf_close(obuf);

    // pop checks
    uint32_t crc = obuf_pop(obuf);
    assert (crc == crc_compute(msg, strlen(msg)));

    // check empty
    assert (obuf_size(obuf) == 0);

    // clean up
    obuf_fini(obuf);
}


void
two_messages()
{
    obuf_t* obuf = obuf_init(10);
    char* msg = "hello world"; // complete message
    char* msg1 = "hello ";
    char* msg2 = "world";
    char* msg3 = "hallo mama"; // other message

    // first execution
    obuf_push(obuf, msg3, strlen(msg3));
    obuf_done(obuf);

    obuf_push(obuf, msg1, strlen(msg1));
    obuf_push(obuf, msg2, strlen(msg2));
    obuf_done(obuf);

    obuf_close(obuf);

    // second execution
    obuf_push(obuf, msg3, strlen(msg3));
    obuf_done(obuf);

    obuf_push(obuf, msg1, strlen(msg1));
    obuf_push(obuf, msg2, strlen(msg2));
    obuf_done(obuf);
    obuf_close(obuf);

    // pop checks
    uint32_t crc = obuf_pop(obuf);
    assert (crc == crc_compute(msg3, strlen(msg3)));

    crc = obuf_pop(obuf);
    assert (crc == crc_compute(msg, strlen(msg)));

    // check empty
    assert (obuf_size(obuf) == 0);

    // clean up
    obuf_fini(obuf);
}

int
main(int argc, char* argv[])
{
    init_fini();
    one_message();
    two_messages();
    two_part_message();
    return 0;
}
