/* ----------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <asco.h>

#include "types.h"
#include "foo.h"

output_t*
handler(input_t* input, output_t* output)
{
    __transaction_atomic {
        if (!output)
            // If state (ie, output) == NULL, allocate it. This will
            // be executed only the first time entry is called.
            output = malloc(sizeof(output_t));
        output->a = input->a;
        foo(output);
        return output;
    }
}

asco_t* asco = NULL;

int
main()
{
    // Call asco_init to intialize the 2 heaps (for p and q).
    asco = asco_init();

    // input variable
    input_t input;

    // This will be used as output and state in this example both
    // start as empty.
    output_t* output1 = NULL;
    output_t* output2 = NULL;

    // loop for 10 input values
    for (input.a = 1; input.a <= 10; input.a++) {
        // asco_begin says next heap is p.
        asco_begin(asco);

        // Call event handler with input and old state (output1).
        output1 = handler(&input, output1);

        // Show output1 before applying COW back to the heap.
        printf("Before apply %d = %d\n", input.a, output1->a);

        // Switch to q.
        asco_switch(asco);

        // Call event handler with input and old state (output2).
        output2 = handler(&input, output2);

        // Show output2 before applying COW back to the heap. Note
        // that both heaps use a COW buffer.
        printf("After switch %d = %d\n", input.a, output2->a);

        // Apply the COWs to the heaps. Check whether values in COW
        // are the same.
        asco_commit(asco);
        printf("After apply %d = %d = %d\n", input.a, output1->a, output2->a);

        // Since output messages were allocated in the heaps, one
        // could think that the comparison between COWs in
        // asco_commit() would be sufficient to check that both
        // outputs are the same. I guess that this is not enough if
        // the message content was not modified during the handler
        // execution. Perhaps it is a default message placed in the
        // heap. Therefore we should compare both outputs here as well.
        output_t* t1 = output1;
        output_t* t2 = output2;
        for (; t1 && t2; t1 = t1->next, t2 = t2->next)
            assert (t1->a == t2->a);
    }

    // Delete heaps.
    asco_fini(asco);
    return 0;
}


/* This is the output of the last iteration:


   Before apply 10 = 9
   After switch 10 = 9
   ----------
   COW BUFFER 0:                           << PASC-lib output
   addr =   0x7f3fe848d030 value = 10      << PASC-lib output
   ----------
   ----------
   COW BUFFER 0:                           << PASC-lib output
   addr =   0x7f3fe7a8c030 value = 10      << PASC-lib output
   ----------
   After apply 10 = 10 = 10
 */
