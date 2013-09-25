/* ----------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */
#include <tmasco.h>
#include <stdlib.h>
#include "types.h"
#include "foo.h"

output_t*
foo(state_t* state, input_t* input)
{
    state->sum += input->a;
    output_t* output = (output_t*) malloc(sizeof(output_t));
    output->a   = input->a + state->sum;

    // output is a message to be sent
    __asco_output_append(output, sizeof(output_t));
    // the message has only one part
    __asco_output_done();

    return output;
}
