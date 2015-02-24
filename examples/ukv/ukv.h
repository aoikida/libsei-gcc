/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _UKV_H_
#define _UKV_H_
#include <sei.h>

struct ukv;
typedef struct ukv ukv_t;

ukv_t*      ukv_init() SEI_SAFE;
void        ukv_fini(ukv_t* ukv) SEI_SAFE;
const char* ukv_set (ukv_t* ukv, char* key, char* value) SEI_SAFE;
const char* ukv_get (ukv_t* ukv, const char* key) SEI_SAFE;
void        ukv_del (ukv_t* ukv, const char* key) SEI_SAFE;

#endif /* _UKV_H_ */
