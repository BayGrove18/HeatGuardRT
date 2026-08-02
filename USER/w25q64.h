#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>

void w25q64_init(void);
uint32_t w25q64_read_jedec_id(void);
uint8_t w25q64_read(uint32_t address, uint8_t *buffer, uint32_t length);
uint8_t w25q64_write(uint32_t address, const uint8_t *buffer, uint32_t length);
uint8_t w25q64_erase_range(uint32_t address, uint32_t length);

#endif
