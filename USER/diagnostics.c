#include "diagnostics.h"

#include "task.h"

volatile AppDiagnostics g_app_diagnostics;
static QueueHandle_t diagnostics_queue;

void diagnostics_init(QueueHandle_t event_queue)
{
    diagnostics_queue = event_queue;
    g_app_diagnostics.queue_min_free_slots = UINT16_MAX;
}

void diagnostics_record_event_overflow(void)
{
    ++g_app_diagnostics.event_overflow_count;
}

void diagnostics_refresh(const ControlSnapshot *snapshot)
{
    UBaseType_t queue_free_slots;
    UBaseType_t control_stack_min_words;
    size_t free_heap_bytes;

    if (diagnostics_queue != NULL) {
        queue_free_slots = uxQueueSpacesAvailable(diagnostics_queue);
        if (queue_free_slots < g_app_diagnostics.queue_min_free_slots) {
            g_app_diagnostics.queue_min_free_slots = (uint16_t)queue_free_slots;
        }
    }

    control_stack_min_words = uxTaskGetStackHighWaterMark(NULL);
    if (g_app_diagnostics.control_stack_min_words == 0U ||
        control_stack_min_words < g_app_diagnostics.control_stack_min_words) {
        g_app_diagnostics.control_stack_min_words = (uint16_t)control_stack_min_words;
    }

    free_heap_bytes = xPortGetFreeHeapSize();
    g_app_diagnostics.free_heap_bytes = free_heap_bytes > UINT16_MAX ?
                                        UINT16_MAX : (uint16_t)free_heap_bytes;
    g_app_diagnostics.state = (uint8_t)snapshot->state;
    g_app_diagnostics.fault = (uint8_t)snapshot->fault;
    g_app_diagnostics.sensor_error_count = snapshot->sensor_error_count;
    g_app_diagnostics.last_temperature_c = snapshot->last_temperature_c;
}
