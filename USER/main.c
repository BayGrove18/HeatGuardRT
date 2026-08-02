#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "actuator.h"
#include "beep.h"
#include "boot_handoff.h"
#include "control.h"
#include "delay.h"
#include "diagnostics.h"
#include "dht11.h"
#include "exti.h"
#include "gui.h"
#include "heatguard_config.h"
#include "lcd.h"
#include "led.h"
#include "spi_bus.h"
#include "supervisor.h"
#include "timer.h"
#include "upgrade.h"
#include "usart.h"
#include "w25q64.h"

#define CONTROL_TASK_PRIORITY 6U
#define START_TASK_PRIORITY 5U
#define MODE_TASK_PRIORITY 4U
#define SENSOR_TASK_PRIORITY 3U
#define SUPERVISOR_TASK_PRIORITY 2U
#define UPGRADE_TASK_PRIORITY 1U
#define BOOT_TASK_PRIORITY 1U

#define CONTROL_TASK_STACK_WORDS 256U
#define START_TASK_STACK_WORDS 128U
#define MODE_TASK_STACK_WORDS 128U
#define SENSOR_TASK_STACK_WORDS 192U
#define SUPERVISOR_TASK_STACK_WORDS 128U
#define UPGRADE_TASK_STACK_WORDS 256U
#define BOOT_TASK_STACK_WORDS 128U

#define CONTROL_QUEUE_WAIT_TICKS pdMS_TO_TICKS(HEATGUARD_EVENT_QUEUE_WAIT_MS)
#define CONTROL_HEARTBEAT_TICKS pdMS_TO_TICKS(100U)
#define KEY_DEBOUNCE_TICKS pdMS_TO_TICKS(HEATGUARD_KEY_DEBOUNCE_MS)
#define KEY_SAMPLE_TICKS pdMS_TO_TICKS(HEATGUARD_KEY_SAMPLE_MS)
#define KEY_LONG_PRESS_TICKS pdMS_TO_TICKS(HEATGUARD_KEY_LONG_PRESS_MS)
#define SENSOR_PERIOD_TICKS pdMS_TO_TICKS(HEATGUARD_SENSOR_PERIOD_MS)

static QueueHandle_t control_queue;
static TaskHandle_t start_task_handle;
static TaskHandle_t mode_task_handle;
static volatile uint8_t event_overflow;

static led_d status_led;
led_d bep;
static led_d dht11_pin;

static void boot_task(void *argument);
static void control_task(void *argument);
static void sensor_task(void *argument);
static void start_task(void *argument);
static void mode_task(void *argument);

static uint8_t pin_is_low(uint16_t pin)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIOB, pin) == Bit_RESET);
}

static void post_event(const ControlEvent *event)
{
    if (xQueueSend(control_queue, event, CONTROL_QUEUE_WAIT_TICKS) != pdPASS) {
        event_overflow = 1U;
        actuator_force_safe();
    }
}

static void post_event_from_isr(const ControlEvent *event,
                                BaseType_t *higher_priority_task_woken)
{
    if (xQueueSendFromISR(control_queue, event, higher_priority_task_woken) != pdPASS) {
        event_overflow = 1U;
        actuator_force_safe();
    }
}

static uint8_t take_event_overflow(void)
{
    uint8_t overflow;

    taskENTER_CRITICAL();
    overflow = event_overflow;
    event_overflow = 0U;
    taskEXIT_CRITICAL();
    return overflow;
}

static const char *power_text(ControlPower power)
{
    switch (power) {
    case CONTROL_POWER_LOW: return "Low";
    case CONTROL_POWER_HIGH: return "High";
    case CONTROL_POWER_MEDIUM:
    default: return "Medium";
    }
}

static void apply_outputs(const ControlSnapshot *snapshot)
{
    actuator_apply(snapshot);
    if (snapshot->heater_enabled != 0U) {
        led_off(&status_led);
        Beep_off(&bep);
    } else {
        led_on(&status_led);
        if (snapshot->state == CONTROL_STATE_FAULT) {
            Beep_on(&bep);
        } else {
            Beep_off(&bep);
        }
    }
}

static void refresh_display(const ControlSnapshot *snapshot)
{
    if (spi_bus_lock(pdMS_TO_TICKS(10U)) != pdPASS) {
        return;
    }
    LCD_Fill(0U, 20U, 128U, 160U, WHITE);
    Show_Str(0U, 20U, BLUE, WHITE, (u8 *)control_state_text(snapshot->state), 16U, 0U);
    Show_Str(0U, 45U, BLUE, WHITE, "Time:    s", 16U, 0U);
    LCD_ShowNum(40U, 45U, snapshot->cooking_seconds, 3U, 16U);
    Show_Str(0U, 70U, BLUE, WHITE, "Power:", 16U, 0U);
    Show_Str(48U, 70U, RED, WHITE, (u8 *)power_text(snapshot->power), 16U, 0U);
    Show_Str(0U, 95U, BLUE, WHITE,
             (u8 *)((snapshot->door_closed != 0U) ? "Door: closed" : "Door: open"), 16U, 0U);
    Show_Str(0U, 120U, BLUE, WHITE, "Temp:    C", 16U, 0U);
    LCD_ShowNum(40U, 120U, snapshot->last_temperature_c, 2U, 16U);
    if (snapshot->state == CONTROL_STATE_FAULT) {
        Show_Str(0U, 145U, RED, WHITE,
                 (u8 *)control_fault_text(snapshot->fault), 16U, 0U);
    } else if (snapshot->state == CONTROL_STATE_UPDATE_PENDING) {
        Show_Str(0U, 145U, RED, WHITE, "Boot handoff", 16U, 0U);
    }
    spi_bus_unlock();
}

static void publish_snapshot(const ControlSnapshot *snapshot)
{
    apply_outputs(snapshot);
    refresh_display(snapshot);
}

static void request_upgrade_safe_state(void)
{
    ControlEvent event;

    event.type = CONTROL_EVENT_UPDATE_REQUEST;
    event.value = 0U;
    post_event(&event);
}

static void control_task(void *argument)
{
    ControlSnapshot snapshot;
    ControlEvent event;

    (void)argument;
    control_init(&snapshot);
    publish_snapshot(&snapshot);
    diagnostics_refresh(&snapshot);

    for (;;) {
        if (take_event_overflow() != 0U) {
            event.type = CONTROL_EVENT_SYSTEM_FAULT;
            event.value = 0U;
            diagnostics_record_event_overflow();
            (void)control_dispatch(&snapshot, &event);
            publish_snapshot(&snapshot);
            diagnostics_refresh(&snapshot);
        }
        if (xQueueReceive(control_queue, &event, CONTROL_HEARTBEAT_TICKS) == pdTRUE) {
            if (control_dispatch(&snapshot, &event) != 0U) {
                publish_snapshot(&snapshot);
            }
            diagnostics_refresh(&snapshot);
            if (snapshot.state == CONTROL_STATE_UPDATE_PENDING) {
                upgrade_commit_after_safe_state();
            }
        }
        supervisor_control_heartbeat();
    }
}

static void sensor_task(void *argument)
{
    TickType_t wake_time = xTaskGetTickCount();
    ControlEvent event;
    u8 temperature;
    u8 humidity;

    (void)argument;
    for (;;) {
        if (DHT_Read_Data(&temperature, &humidity, GPIOC, GPIO_Pin_14, &dht11_pin) != 0U) {
            event.type = CONTROL_EVENT_TEMPERATURE;
            event.value = temperature;
        } else {
            event.type = CONTROL_EVENT_SENSOR_ERROR;
            event.value = 0U;
        }
        post_event(&event);
        vTaskDelayUntil(&wake_time, SENSOR_PERIOD_TICKS);
    }
}

static void start_task(void *argument)
{
    ControlEvent event;

    (void)argument;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(KEY_DEBOUNCE_TICKS);
        if (pin_is_low(HEATGUARD_START_PIN) != 0U) {
            event.type = CONTROL_EVENT_START_REQUEST;
            event.value = 0U;
            post_event(&event);
            while (pin_is_low(HEATGUARD_START_PIN) != 0U) {
                vTaskDelay(KEY_SAMPLE_TICKS);
            }
        }
    }
}

static void mode_task(void *argument)
{
    ControlEvent event;
    TickType_t pressed_at;
    uint8_t pressed;

    (void)argument;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(KEY_DEBOUNCE_TICKS);
        if (pin_is_low(HEATGUARD_MODE_PIN) == 0U) {
            continue;
        }
        pressed_at = xTaskGetTickCount();
        pressed = 1U;
        while (pressed != 0U &&
               (xTaskGetTickCount() - pressed_at) < KEY_LONG_PRESS_TICKS) {
            vTaskDelay(KEY_SAMPLE_TICKS);
            pressed = pin_is_low(HEATGUARD_MODE_PIN);
        }
        event.type = pressed != 0U ? CONTROL_EVENT_MODE_LONG : CONTROL_EVENT_MODE_SHORT;
        event.value = 0U;
        post_event(&event);
        while (pressed != 0U) {
            vTaskDelay(KEY_SAMPLE_TICKS);
            pressed = pin_is_low(HEATGUARD_MODE_PIN);
        }
    }
}

static void boot_task(void *argument)
{
    BaseType_t result;
    ControlEvent initial_door_event;

    (void)argument;
    result = xTaskCreate(control_task, "control", CONTROL_TASK_STACK_WORDS,
                         NULL, CONTROL_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(start_task, "start", START_TASK_STACK_WORDS,
                         NULL, START_TASK_PRIORITY, &start_task_handle);
    configASSERT(result == pdPASS);
    result = xTaskCreate(mode_task, "mode", MODE_TASK_STACK_WORDS,
                         NULL, MODE_TASK_PRIORITY, &mode_task_handle);
    configASSERT(result == pdPASS);
    result = xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK_WORDS,
                         NULL, SENSOR_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(supervisor_task, "supervisor", SUPERVISOR_TASK_STACK_WORDS,
                         NULL, SUPERVISOR_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(upgrade_task, "upgrade", UPGRADE_TASK_STACK_WORDS,
                         NULL, UPGRADE_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);

    initial_door_event.type = CONTROL_EVENT_DOOR_CHANGED;
    initial_door_event.value = pin_is_low(HEATGUARD_DOOR_PIN);
    post_event(&initial_door_event);
    vTaskDelete(NULL);
}

int main(void)
{
    BaseType_t result;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    boot_handoff_init();
    diagnostics_set_reset_cause((uint8_t)boot_handoff_reset_cause());
    delay_init();
    uart_init(115200U);
    LED_Init(&status_led, GPIOC, GPIO_Pin_13);
    Beep_Init(&bep, GPIOB, GPIO_Pin_14);
    actuator_init();
    LCD_Init();
    spi_bus_init();
    w25q64_init();
    upgrade_init(request_upgrade_safe_state);

    control_queue = xQueueCreate(HEATGUARD_EVENT_QUEUE_LENGTH, sizeof(ControlEvent));
    configASSERT(control_queue != NULL);
    diagnostics_init(control_queue);
    EXTIX_Init();
    supervisor_init();

    result = xTaskCreate(boot_task, "boot", BOOT_TASK_STACK_WORDS,
                         NULL, BOOT_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    vTaskStartScheduler();
    for (;;) {
    }
}

void EXTI1_IRQHandler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        if (mode_task_handle != NULL) {
            vTaskNotifyGiveFromISR(mode_task_handle, &higher_priority_task_woken);
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

void EXTI15_10_IRQHandler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    ControlEvent event;

    if (EXTI_GetITStatus(EXTI_Line10) != RESET) {
        if (start_task_handle != NULL) {
            vTaskNotifyGiveFromISR(start_task_handle, &higher_priority_task_woken);
        }
        EXTI_ClearITPendingBit(EXTI_Line10);
    }
    if (EXTI_GetITStatus(EXTI_Line12) != RESET) {
        event.type = CONTROL_EVENT_DOOR_CHANGED;
        event.value = pin_is_low(HEATGUARD_DOOR_PIN);
        if (event.value == 0U) {
            actuator_force_safe();
        }
        post_event_from_isr(&event, &higher_priority_task_woken);
        EXTI_ClearITPendingBit(EXTI_Line12);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void TIM4_IRQHandler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    ControlEvent event;

    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        event.type = CONTROL_EVENT_TICK;
        event.value = 0U;
        post_event_from_isr(&event, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    actuator_force_safe();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    actuator_force_safe();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationAssert(const char *file, int line)
{
    (void)file;
    (void)line;
    actuator_force_safe();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
