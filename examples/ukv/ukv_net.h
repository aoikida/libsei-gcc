/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _UKV_NET_H_
#define _UKV_NET_H_

#include "ukv.h"

const char* ukv_recv(ukv_t* ukv, const char* msg);
void ukv_done(ukv_t* ukv, const char* reply);

#endif /* _UKV_NET_H_ */
