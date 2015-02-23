/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _SEI_CFC_H_
#define _SEI_CFC_H_

typedef enum {SET, RESET} cfc_flag_t;

typedef struct cfc {
    cfc_flag_t Scf;
    cfc_flag_t Rcf;
    cfc_flag_t LScf;
    cfc_flag_t LRcf;
} cfc_t;

void cfc_reset(cfc_t* cfc);
void cfc_alog (cfc_t* cfc); // at-least-once gate
int  cfc_amog (cfc_t* cfc); // at-most-once gate
int  cfc_check(cfc_t* cfc); // at-least-once check

#endif /* _SEI_CFC_H_ */
