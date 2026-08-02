#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

typedef enum {
    CONTROL_STATE_STANDBY = 0,
    CONTROL_STATE_TIME_SETTING,
    CONTROL_STATE_POWER_SETTING,
    CONTROL_STATE_HEATING,
    CONTROL_STATE_COMPLETED,
    CONTROL_STATE_FAULT,
    CONTROL_STATE_UPDATE_PENDING
} ControlState;

typedef enum {
    CONTROL_POWER_LOW = 0,
    CONTROL_POWER_MEDIUM,
    CONTROL_POWER_HIGH
} ControlPower;

typedef enum {
    CONTROL_FAULT_NONE = 0,
    CONTROL_FAULT_OVERTEMPERATURE,
    CONTROL_FAULT_SENSOR_TIMEOUT,
    CONTROL_FAULT_EVENT_OVERFLOW
} ControlFault;

typedef enum {
    CONTROL_EVENT_DOOR_CHANGED = 0,
    CONTROL_EVENT_START_REQUEST,
    CONTROL_EVENT_MODE_SHORT,
    CONTROL_EVENT_MODE_LONG,
    CONTROL_EVENT_TICK,
    CONTROL_EVENT_TEMPERATURE,
    CONTROL_EVENT_SENSOR_ERROR,
    CONTROL_EVENT_UPDATE_REQUEST,
    CONTROL_EVENT_SYSTEM_FAULT
} ControlEventType;

typedef struct {
    ControlEventType type;
    uint16_t value;
} ControlEvent;

typedef struct {
    ControlState state;
    ControlPower power;
    ControlFault fault;
    uint16_t cooking_seconds;
    uint8_t door_closed;
    uint8_t sensor_valid;
    uint8_t heater_enabled;
    uint8_t last_temperature_c;
    uint8_t sensor_error_count;
} ControlSnapshot;

void control_init(ControlSnapshot *snapshot);
uint8_t control_dispatch(ControlSnapshot *snapshot, const ControlEvent *event);
uint8_t control_heating_permitted(const ControlSnapshot *snapshot);
const char *control_state_text(ControlState state);
const char *control_fault_text(ControlFault fault);

#endif
