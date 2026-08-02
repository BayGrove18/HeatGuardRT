#include "crc32.h"

uint32_t crc32_init(void)
{
    return 0xFFFFFFFFUL;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t index;

    for (index = 0U; index < length; ++index) {
        uint8_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1UL) != 0UL ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        }
    }
    return crc;
}

uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

uint32_t crc32_calculate(const uint8_t *data, uint32_t length)
{
    return crc32_finalize(crc32_update(crc32_init(), data, length));
}
