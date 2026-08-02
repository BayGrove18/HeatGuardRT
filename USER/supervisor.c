#include "supervisor.h"

#include "task.h"
#include "stm32f10x_iwdg.h"

#include "actuator.h"
#include "heatguard_config.h"

static volatile uint32_t control_heartbeat;

void supervisor_init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(1000U);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

void supervisor_control_heartbeat(void)
{
    ++control_heartbeat;
}

void supervisor_task(void *argument)
{
    uint32_t last_heartbeat = 0U;

    (void)argument;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEATGUARD_SUPERVISOR_PERIOD_MS));
        if (control_heartbeat != last_heartbeat) {
            last_heartbeat = control_heartbeat;
            IWDG_ReloadCounter();
        } else {
            actuator_force_safe();
        }
    }
}
