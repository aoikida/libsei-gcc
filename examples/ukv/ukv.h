/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _UKV_H_
#define _UKV_H_

struct ukv;
typedef struct ukv ukv_t;

ukv_t*      ukv_init();
void        ukv_fini(ukv_t* ukv);
const char* ukv_set (ukv_t* ukv, char* key, char* value);
const char* ukv_get (ukv_t* ukv, const char* key);
void        ukv_del (ukv_t* ukv, const char* key);

#endif /* _UKV_H_ */
