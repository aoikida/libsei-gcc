/* ----------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------- */
#ifndef _TYPES_H
#define _TYPES_H

// some input value
typedef struct input {
    int a;
} input_t;

// output can be a linked list of output messages
typedef struct output {
    int a;
    struct output* next;
} output_t;

#endif
