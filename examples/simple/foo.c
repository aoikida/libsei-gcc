/* ----------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */
#include <stdlib.h>
#include "types.h"
#include "foo.h"

// add a new output message
void
foo(output_t* output)
{
    int sum = 0;
    // Sum up the complete list and create a new output message with
    // the sum. Append that to the state (ie, output).
    while (output->next != NULL) {
        sum += output->a;
        output = output->next;
    }
    output->next = (output_t*) malloc(sizeof(output_t));
    output->next->a = output->a + sum;
}
