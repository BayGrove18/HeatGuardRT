#ifndef ACTUATOR_H
#define ACTUATOR_H

#include "control.h"

void actuator_init(void);
void actuator_apply(const ControlSnapshot *snapshot);
void actuator_force_safe(void);

#endif
