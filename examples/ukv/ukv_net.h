/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _UKV_NET_H_
#define _UKV_NET_H_

#include "ukv.h"

const char* ukv_recv(ukv_t* ukv, const char* msg) SEI_SAFE;
void        ukv_done(ukv_t* ukv, const char* reply) SEI_SAFE;

#endif /* _UKV_NET_H_ */
