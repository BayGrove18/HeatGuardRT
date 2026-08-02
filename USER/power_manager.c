#include "power_manager.h"

#include "task.h"

#include "diagnostics.h"
#include "heatguard_config.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"
#include "system_stm32f10x.h"

#define POWER_MANAGER_RTC_LSE_MAGIC 0x504CU
#define POWER_MANAGER_RTC_LSI_MAGIC 0x5053U
#define POWER_MANAGER_LCD_BACKLIGHT_PIN GPIO_Pin_6
#define POWER_MANAGER_LSE_STARTUP_LIMIT 0x80000UL

static volatile ControlState control_state = CONTROL_STATE_STANDBY;
static volatile uint8_t upgrade_active;
static uint32_t rtc_ticks_per_second = HEATGUARD_RTC_TICKS_PER_SECOND;

static uint8_t power_manager_can_stop(void)
{
    if (upgrade_active != 0U) {
        return 0U;
    }
    return (uint8_t)(control_state == CONTROL_STATE_STANDBY ||
                     control_state == CONTROL_STATE_TIME_SETTING ||
                     control_state == CONTROL_STATE_POWER_SETTING ||
                     control_state == CONTROL_STATE_COMPLETED);
}

static void power_manager_rtc_init(void)
{
    uint32_t timeout = POWER_MANAGER_LSE_STARTUP_LIMIT;

    if (BKP_ReadBackupRegister(BKP_DR3) == POWER_MANAGER_RTC_LSE_MAGIC) {
        rtc_ticks_per_second = HEATGUARD_RTC_TICKS_PER_SECOND;
        RTC_WaitForSynchro();
        return;
    }
    if (BKP_ReadBackupRegister(BKP_DR3) == POWER_MANAGER_RTC_LSI_MAGIC) {
        rtc_ticks_per_second = 1000UL;
        RTC_WaitForSynchro();
        return;
    }

    RCC_LSEConfig(RCC_LSE_ON);
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET && timeout-- != 0U) {
    }
    if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET) {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        rtc_ticks_per_second = HEATGUARD_RTC_TICKS_PER_SECOND;
    } else {
        RCC_LSICmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET) {
        }
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
        rtc_ticks_per_second = 1000UL;
    }
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_SetPrescaler((rtc_ticks_per_second == HEATGUARD_RTC_TICKS_PER_SECOND) ?
                     31U : 39U);
    RTC_WaitForLastTask();
    RTC_SetCounter(0U);
    RTC_WaitForLastTask();
    BKP_WriteBackupRegister(BKP_DR3,
                            rtc_ticks_per_second == HEATGUARD_RTC_TICKS_PER_SECOND ?
                            POWER_MANAGER_RTC_LSE_MAGIC : POWER_MANAGER_RTC_LSI_MAGIC);
}

void power_manager_init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    power_manager_rtc_init();

    exti.EXTI_Line = EXTI_Line17;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    RTC_ITConfig(RTC_IT_ALR, ENABLE);

    nvic.NVIC_IRQChannel = RTCAlarm_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 5;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void power_manager_set_control_state(ControlState state)
{
    control_state = state;
}

void power_manager_set_upgrade_active(uint8_t active)
{
    upgrade_active = active;
}

void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks)
{
    uint32_t rtc_start;
    uint32_t rtc_elapsed;
    TickType_t elapsed_ticks;

    if (expected_idle_ticks < HEATGUARD_STOP_MIN_IDLE_TICKS ||
        power_manager_can_stop() == 0U) {
        __wfi();
        return;
    }
    if (expected_idle_ticks > HEATGUARD_MAX_STOP_TICKS) {
        expected_idle_ticks = HEATGUARD_MAX_STOP_TICKS;
    }

    __disable_irq();
    if (eTaskConfirmSleepModeStatus() == eAbortSleep) {
        __enable_irq();
        return;
    }

    rtc_start = RTC_GetCounter();
    RTC_ClearITPendingBit(RTC_IT_ALR);
    EXTI_ClearITPendingBit(EXTI_Line17);
    RTC_WaitForLastTask();
    RTC_SetAlarm(rtc_start +
                 ((uint32_t)expected_idle_ticks * rtc_ticks_per_second +
                  configTICK_RATE_HZ - 1UL) / configTICK_RATE_HZ);
    RTC_WaitForLastTask();
    SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
    GPIO_ResetBits(GPIOB, POWER_MANAGER_LCD_BACKLIGHT_PIN);

    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    SystemClock_RestoreAfterStop();
    GPIO_SetBits(GPIOB, POWER_MANAGER_LCD_BACKLIGHT_PIN);
    rtc_elapsed = RTC_GetCounter() - rtc_start;
    elapsed_ticks = (TickType_t)((rtc_elapsed * configTICK_RATE_HZ) /
                                 rtc_ticks_per_second);
    if (elapsed_ticks > expected_idle_ticks) {
        elapsed_ticks = expected_idle_ticks;
    }
    RTC_ClearITPendingBit(RTC_IT_ALR);
    EXTI_ClearITPendingBit(EXTI_Line17);
    SysTick->LOAD = (SystemCoreClock / configTICK_RATE_HZ) - 1UL;
    SysTick->VAL = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    if (elapsed_ticks != 0U) {
        vTaskStepTick(elapsed_ticks);
    }
    diagnostics_record_stop(elapsed_ticks);
    __enable_irq();
}

void RTCAlarm_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_ALR);
        EXTI_ClearITPendingBit(EXTI_Line17);
    }
}
