/* -----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * -------------------------------------------------------------------------- */
#ifndef _ILOG_H_
#define _ILOG_H_

typedef struct ilog ilog_t;

ilog_t* ilog_init(const char* fname);
void    ilog_fini(ilog_t* ilog);
void    ilog_push(ilog_t* ilog, const char* topic, const char* info);

#endif /* _ILOG_H_ */
