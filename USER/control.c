#include "control.h"
#include "heatguard_config.h"

static void control_stop_heating(ControlSnapshot *snapshot)
{
    snapshot->heater_enabled = 0U;
    if (snapshot->state == CONTROL_STATE_HEATING) {
        snapshot->state = CONTROL_STATE_STANDBY;
    }
}

static void control_enter_fault(ControlSnapshot *snapshot, ControlFault fault)
{
    snapshot->heater_enabled = 0U;
    snapshot->fault = fault;
    snapshot->state = CONTROL_STATE_FAULT;
}

void control_init(ControlSnapshot *snapshot)
{
    snapshot->state = CONTROL_STATE_STANDBY;
    snapshot->power = CONTROL_POWER_MEDIUM;
    snapshot->fault = CONTROL_FAULT_NONE;
    snapshot->cooking_seconds = 0U;
    snapshot->door_closed = 0U;
    snapshot->sensor_valid = 0U;
    snapshot->heater_enabled = 0U;
    snapshot->last_temperature_c = 0U;
    snapshot->sensor_error_count = 0U;
}

uint8_t control_heating_permitted(const ControlSnapshot *snapshot)
{
    return (snapshot->door_closed != 0U &&
            snapshot->sensor_valid != 0U &&
            snapshot->fault == CONTROL_FAULT_NONE &&
            snapshot->cooking_seconds != 0U);
}

uint8_t control_dispatch(ControlSnapshot *snapshot, const ControlEvent *event)
{
    ControlSnapshot before = *snapshot;

    switch (event->type) {
    case CONTROL_EVENT_DOOR_CHANGED:
        snapshot->door_closed = event->value != 0U ? 1U : 0U;
        if (snapshot->door_closed == 0U) {
            control_stop_heating(snapshot);
        }
        if (snapshot->state == CONTROL_STATE_COMPLETED) {
            snapshot->state = CONTROL_STATE_STANDBY;
        }
        break;

    case CONTROL_EVENT_START_REQUEST:
        if (snapshot->state != CONTROL_STATE_FAULT &&
            snapshot->state != CONTROL_STATE_UPDATE_PENDING &&
            control_heating_permitted(snapshot) != 0U) {
            snapshot->state = CONTROL_STATE_HEATING;
            snapshot->heater_enabled = 1U;
        }
        break;

    case CONTROL_EVENT_MODE_SHORT:
        if (snapshot->state == CONTROL_STATE_STANDBY ||
            snapshot->state == CONTROL_STATE_COMPLETED) {
            snapshot->state = CONTROL_STATE_TIME_SETTING;
        } else if (snapshot->state == CONTROL_STATE_TIME_SETTING) {
            snapshot->cooking_seconds = (uint16_t)(snapshot->cooking_seconds +
                                                   HEATGUARD_COOKING_TIME_STEP_SECONDS);
            if (snapshot->cooking_seconds > HEATGUARD_MAX_COOKING_SECONDS) {
                snapshot->cooking_seconds = 0U;
            }
        } else if (snapshot->state == CONTROL_STATE_POWER_SETTING) {
            snapshot->power = (ControlPower)((snapshot->power + 1U) % 3U);
        }
        break;

    case CONTROL_EVENT_MODE_LONG:
        if (snapshot->state == CONTROL_STATE_TIME_SETTING) {
            snapshot->state = CONTROL_STATE_POWER_SETTING;
        } else if (snapshot->state == CONTROL_STATE_POWER_SETTING) {
            snapshot->state = CONTROL_STATE_TIME_SETTING;
        }
        break;

    case CONTROL_EVENT_TICK:
        if (snapshot->state == CONTROL_STATE_HEATING) {
            if (snapshot->cooking_seconds > 0U) {
                --snapshot->cooking_seconds;
            }
            if (snapshot->cooking_seconds == 0U) {
                snapshot->heater_enabled = 0U;
                snapshot->state = CONTROL_STATE_COMPLETED;
            }
        }
        break;

    case CONTROL_EVENT_TEMPERATURE:
        snapshot->last_temperature_c = (uint8_t)event->value;
        snapshot->sensor_error_count = 0U;
        snapshot->sensor_valid = 1U;
        if (snapshot->state == CONTROL_STATE_HEATING &&
            snapshot->last_temperature_c >= HEATGUARD_OVERTEMPERATURE_C) {
            control_enter_fault(snapshot, CONTROL_FAULT_OVERTEMPERATURE);
        }
        break;

    case CONTROL_EVENT_SENSOR_ERROR:
        if (snapshot->sensor_error_count < UINT8_MAX) {
            ++snapshot->sensor_error_count;
        }
        if (snapshot->sensor_error_count >= HEATGUARD_MAX_SENSOR_ERRORS) {
            snapshot->sensor_valid = 0U;
            if (snapshot->state == CONTROL_STATE_HEATING) {
                control_enter_fault(snapshot, CONTROL_FAULT_SENSOR_TIMEOUT);
            }
        }
        break;

    case CONTROL_EVENT_UPDATE_REQUEST:
        snapshot->heater_enabled = 0U;
        snapshot->state = CONTROL_STATE_UPDATE_PENDING;
        break;

    case CONTROL_EVENT_SYSTEM_FAULT:
        control_enter_fault(snapshot, CONTROL_FAULT_EVENT_OVERFLOW);
        break;

    default:
        break;
    }

    return (uint8_t)(before.state != snapshot->state ||
                     before.power != snapshot->power ||
                     before.fault != snapshot->fault ||
                     before.cooking_seconds != snapshot->cooking_seconds ||
                     before.door_closed != snapshot->door_closed ||
                     before.sensor_valid != snapshot->sensor_valid ||
                     before.heater_enabled != snapshot->heater_enabled ||
                     before.last_temperature_c != snapshot->last_temperature_c ||
                     before.sensor_error_count != snapshot->sensor_error_count);
}

const char *control_state_text(ControlState state)
{
    switch (state) {
    case CONTROL_STATE_STANDBY: return "Standby";
    case CONTROL_STATE_TIME_SETTING: return "Set time";
    case CONTROL_STATE_POWER_SETTING: return "Set power";
    case CONTROL_STATE_HEATING: return "Heating";
    case CONTROL_STATE_COMPLETED: return "Completed";
    case CONTROL_STATE_FAULT: return "Fault";
    case CONTROL_STATE_UPDATE_PENDING: return "Update pending";
    default: return "Unknown";
    }
}

const char *control_fault_text(ControlFault fault)
{
    switch (fault) {
    case CONTROL_FAULT_NONE: return "";
    case CONTROL_FAULT_OVERTEMPERATURE: return "Over temperature";
    case CONTROL_FAULT_SENSOR_TIMEOUT: return "Sensor timeout";
    case CONTROL_FAULT_EVENT_OVERFLOW: return "Event overflow";
    default: return "Unknown fault";
    }
}
