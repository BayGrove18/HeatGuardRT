#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

uint32_t crc32_init(void);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length);
uint32_t crc32_finalize(uint32_t crc);
uint32_t crc32_calculate(const uint8_t *data, uint32_t length);

#endif
