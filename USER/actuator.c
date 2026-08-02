#include "actuator.h"

#include "timer.h"

static uint8_t door_known;
static uint8_t door_closed;

static uint16_t heater_pwm(ControlPower power)
{
    switch (power) {
    case CONTROL_POWER_LOW:
        return 50U;
    case CONTROL_POWER_HIGH:
        return 100U;
    case CONTROL_POWER_MEDIUM:
    default:
        return 70U;
    }
}

static void actuator_set_door(uint8_t closed)
{
    TIM_SetCompare2(TIM2, closed != 0U ? 8U : 3U);
    door_closed = closed;
    door_known = 1U;
}

void actuator_force_safe(void)
{
    TIM_SetCompare3(TIM2, 0U);
    TIM_SetCompare3(TIM3, 0U);
    TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
    TIM_Cmd(TIM4, DISABLE);
}

void actuator_init(void)
{
    TIM3_PWM_Init(99U, 14399U);
    TIM2_PWM_Init(99U, 14399U);
    TIM4_Int_Init(10000U - 1U, 7200U - 1U);
    actuator_force_safe();
    actuator_set_door(0U);
}

void actuator_apply(const ControlSnapshot *snapshot)
{
    if (door_known == 0U || door_closed != snapshot->door_closed) {
        actuator_set_door(snapshot->door_closed);
    }

    if (snapshot->heater_enabled != 0U) {
        TIM_SetCompare3(TIM2, heater_pwm(snapshot->power));
        TIM_SetCompare3(TIM3, 4U);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
        TIM_Cmd(TIM4, ENABLE);
    } else {
        actuator_force_safe();
    }
}
