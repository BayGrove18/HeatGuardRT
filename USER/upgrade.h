#ifndef UPGRADE_H
#define UPGRADE_H

#include "FreeRTOS.h"

typedef void (*UpgradeReadyCallback)(void);

void upgrade_init(UpgradeReadyCallback ready_callback);
void upgrade_task(void *argument);
void upgrade_commit_after_safe_state(void);

#endif
