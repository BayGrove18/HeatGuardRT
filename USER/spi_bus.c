#include "spi_bus.h"

#include "semphr.h"

static SemaphoreHandle_t spi_bus_mutex;

void spi_bus_init(void)
{
    spi_bus_mutex = xSemaphoreCreateMutex();
    configASSERT(spi_bus_mutex != NULL);
}

BaseType_t spi_bus_lock(TickType_t timeout)
{
    return xSemaphoreTake(spi_bus_mutex, timeout);
}

void spi_bus_unlock(void)
{
    (void)xSemaphoreGive(spi_bus_mutex);
}
