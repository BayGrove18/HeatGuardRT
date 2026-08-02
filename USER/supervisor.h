#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "FreeRTOS.h"

void supervisor_init(void);
void supervisor_control_heartbeat(void);
void supervisor_task(void *argument);

#endif
