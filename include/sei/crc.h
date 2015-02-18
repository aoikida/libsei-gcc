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

/* get inital CRC */
uint32_t crc_init();

/* append data to CRC */
uint32_t crc_append(uint32_t crc, const char* block, size_t len);

/* append block size to CRC (optional) */
uint32_t crc_append_len(uint32_t crc, size_t tsize);

/* apply last XOR to CRC */
uint32_t crc_close(uint32_t crc);

/* calculate CRC of a single word with initial CRC NULL */
uint32_t crc_word (uint64_t word);

#endif /* _CRC_H_ */
