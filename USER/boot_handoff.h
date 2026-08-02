#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

#include <stdint.h>

#include "handoff_contract.h"

typedef enum {
    APP_RESET_POWER_ON = 0,
    APP_RESET_WATCHDOG,
    APP_RESET_EXTERNAL,
    APP_RESET_SOFTWARE
} AppResetCause;

void boot_handoff_init(void);
AppResetCause boot_handoff_reset_cause(void);
uint8_t boot_handoff_publish(const BootManifest *manifest);
void boot_handoff_commit_and_reset(void);

#endif
