#ifndef UPGRADE_TRANSPORT_H
#define UPGRADE_TRANSPORT_H

#include <stdint.h>

#include "FreeRTOS.h"

void upgrade_transport_init(void);
void upgrade_transport_on_idle_from_isr(void);
void upgrade_transport_on_dma_from_isr(void);
BaseType_t upgrade_transport_receive(uint8_t *byte, TickType_t timeout);
uint8_t upgrade_transport_take_overflow(void);

#endif
