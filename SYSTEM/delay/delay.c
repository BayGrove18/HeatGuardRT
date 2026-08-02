#include "delay.h"

/*
 * SysTick belongs to FreeRTOS after the scheduler starts.  Keep short sensor
 * timing on a dedicated 1 MHz general-purpose timer instead of reprogramming
 * SysTick from task context.
 */
static uint8_t delay_initialized;

void delay_init(void)
{
    TIM_TimeBaseInitTypeDef timer;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    TIM_DeInit(TIM1);

    timer.TIM_Period = 0xFFFF;
    timer.TIM_Prescaler = (uint16_t)(SystemCoreClock / 1000000U - 1U);
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &timer);
    TIM_SetCounter(TIM1, 0U);
    TIM_Cmd(TIM1, ENABLE);

    delay_initialized = 1U;
}

void delay_us(u32 nus)
{
    uint16_t start;

    if (!delay_initialized) {
        delay_init();
    }

    while (nus != 0U) {
        const uint16_t chunk = (nus > 60000U) ? 60000U : (uint16_t)nus;
        start = TIM_GetCounter(TIM1);
        while ((uint16_t)(TIM_GetCounter(TIM1) - start) < chunk) {
        }
        nus -= chunk;
    }
}

void delay_xms(u32 nms)
{
    while (nms != 0U) {
        delay_us(1000U);
        --nms;
    }
}

void delay_ms(u32 nms)
{
    delay_xms(nms);
}
