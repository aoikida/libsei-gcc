/* ----------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */
#include <tmasco.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "types.h"
#include "foo.h"

int printf(const char *format, ...) TMASCO_PURE;
extern uint32_t crc_compute(const char* block, size_t len);

output_t*
handler(state_t* state, input_t* input, uint32_t crc, uint32_t* crco)
{
    // This will be used as output and state in this example both
    // start as empty.
    output_t* output = NULL;

    // at this point we received the message (input, crc)
    // prepare traversal
    __asco_prepare(input, sizeof(input_t), crc, 1);

    // asco_begin says next heap is p.
    __asco_begin(handler);

    // Call event handler with input and state.
    output = foo(state, input);

    // jump back to begin() in first execution
    __asco_end(handler);

    // get message CRC
    *crco = __asco_output_next();

    return output;
}

int
main()
{
    // input variable
    input_t input;

    // state
    state_t* state = (state_t*) malloc(sizeof(state_t));
    state->sum = 0;

    // loop for 10 input values
    for (input.a = 1; input.a <= 1; input.a++) {
        // we receive a message and its CRC

        uint32_t crc = crc_compute((const char*)&input, sizeof(input_t));
        uint32_t crco; // crc of output

        output_t* output = handler(state, &input, crc, &crco);

        // check CRC (this should be done on the receiver)
        uint32_t crcc = crc_compute((const char*)output, sizeof(output_t));
        assert ((!TMASCO_ENABLED || crcc == crco) && "crcs differ!");

        // Show state and messages
        printf("DONE! %d %d %d\n", state->sum, input.a, output->a);

        // we need to free output in a traversal with no message
        __asco_prepare_nm();
        __asco_begin(free);
        free(output);
        __asco_end(free);
    }

    free(state);
    return 0;
}
