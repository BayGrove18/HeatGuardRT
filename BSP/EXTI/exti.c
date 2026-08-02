#include "exti.h"

#include "heatguard_config.h"

void EXTIX_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = HEATGUARD_MODE_PIN | HEATGUARD_START_PIN | HEATGUARD_DOOR_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);
    exti.EXTI_Line = EXTI_Line1;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource10);
    exti.EXTI_Line = EXTI_Line10;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&exti);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
    exti.EXTI_Line = EXTI_Line12;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 5;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 4;
    NVIC_Init(&nvic);
}
