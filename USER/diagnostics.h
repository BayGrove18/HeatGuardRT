#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "control.h"

typedef struct {
    uint32_t event_overflow_count;
    uint16_t queue_min_free_slots;
    uint16_t control_stack_min_words;
    uint16_t free_heap_bytes;
    uint8_t state;
    uint8_t fault;
    uint8_t sensor_error_count;
    uint8_t last_temperature_c;
} AppDiagnostics;

extern volatile AppDiagnostics g_app_diagnostics;

void diagnostics_init(QueueHandle_t event_queue);
void diagnostics_record_event_overflow(void);
void diagnostics_refresh(const ControlSnapshot *snapshot);

#endif
