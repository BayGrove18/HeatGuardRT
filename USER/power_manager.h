#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "control.h"

void power_manager_init(void);
void power_manager_set_control_state(ControlState state);
void power_manager_set_upgrade_active(uint8_t active);
void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks);

#endif
