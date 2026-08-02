#ifndef SPI_BUS_H
#define SPI_BUS_H

#include "FreeRTOS.h"

void spi_bus_init(void);
BaseType_t spi_bus_lock(TickType_t timeout);
void spi_bus_unlock(void);

#endif
