/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>
#include <stddef.h>

/* calculate the complete crc of a block with fixed inital CRC */
uint32_t crc_compute(const char* block, size_t len);

/* calculate CRC with redundancy (returns 1 if all results match, 0 otherwise) */
int crc_compute_redundant(const char* block, size_t len, uint32_t* result,
                          int redundancy_count);

/* get inital CRC */
uint32_t crc_init();

/* append data to CRC */
uint32_t crc_append(uint32_t crc, const char* block, size_t len);
#ifdef COW_WB
uint32_t txcrc_append(uint32_t crc, const char* block, size_t len);
#endif

/* append block size to CRC (optional) */
uint32_t crc_append_len(uint32_t crc, size_t tsize);

/* apply last XOR to CRC */
uint32_t crc_close(uint32_t crc);

/* calculate CRC of a single word with initial CRC NULL */
uint32_t crc_word (uint64_t word);

/* CRC delta of a difference with a reverse offset to the end of the block */
uint32_t crc_delta(uint64_t diff, size_t roff);

#endif /* _CRC_H_ */
