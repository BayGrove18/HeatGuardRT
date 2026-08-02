#include <assert.h>
#include <stdio.h>

#include "control.h"

static void dispatch(ControlSnapshot *snapshot, ControlEventType type, uint16_t value)
{
    ControlEvent event;

    event.type = type;
    event.value = value;
    (void)control_dispatch(snapshot, &event);
}

static void prepare_heating(ControlSnapshot *snapshot)
{
    control_init(snapshot);
    dispatch(snapshot, CONTROL_EVENT_TEMPERATURE, 25U);
    dispatch(snapshot, CONTROL_EVENT_KEY1_SHORT, 0U);
    dispatch(snapshot, CONTROL_EVENT_KEY2_SHORT, 0U);
    dispatch(snapshot, CONTROL_EVENT_KEY2_SHORT, 0U);
    dispatch(snapshot, CONTROL_EVENT_KEY1_LONG, 0U);

    assert(snapshot->state == CONTROL_STATE_HEATING);
    assert(snapshot->heater_enabled != 0U);
}

static void test_door_open_stops_heating(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    dispatch(&snapshot, CONTROL_EVENT_KEY1_SHORT, 0U);
    assert(snapshot.door_closed == 0U);
    assert(snapshot.heater_enabled == 0U);
    assert(snapshot.state == CONTROL_STATE_STANDBY);
}

static void test_overtemperature_latches_fault(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    dispatch(&snapshot, CONTROL_EVENT_TEMPERATURE, 30U);
    assert(snapshot.state == CONTROL_STATE_FAULT);
    assert(snapshot.fault == CONTROL_FAULT_OVERTEMPERATURE);
    assert(snapshot.heater_enabled == 0U);

    dispatch(&snapshot, CONTROL_EVENT_KEY1_LONG, 0U);
    assert(snapshot.state == CONTROL_STATE_FAULT);
    assert(snapshot.heater_enabled == 0U);
}

static void test_sensor_timeout_stops_heating(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    dispatch(&snapshot, CONTROL_EVENT_SENSOR_ERROR, 0U);
    dispatch(&snapshot, CONTROL_EVENT_SENSOR_ERROR, 0U);
    assert(snapshot.state == CONTROL_STATE_HEATING);
    dispatch(&snapshot, CONTROL_EVENT_SENSOR_ERROR, 0U);
    assert(snapshot.state == CONTROL_STATE_FAULT);
    assert(snapshot.fault == CONTROL_FAULT_SENSOR_TIMEOUT);
    assert(snapshot.sensor_valid == 0U);
    assert(snapshot.heater_enabled == 0U);
}

static void test_countdown_completion(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    snapshot.cooking_seconds = 1U;
    dispatch(&snapshot, CONTROL_EVENT_TICK, 0U);
    assert(snapshot.cooking_seconds == 0U);
    assert(snapshot.state == CONTROL_STATE_COMPLETED);
    assert(snapshot.heater_enabled == 0U);
}

static void test_update_request_is_safe(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    dispatch(&snapshot, CONTROL_EVENT_UPDATE_REQUEST, 0U);
    assert(snapshot.state == CONTROL_STATE_UPDATE_PENDING);
    assert(snapshot.heater_enabled == 0U);
}

static void test_system_fault_is_latched(void)
{
    ControlSnapshot snapshot;

    prepare_heating(&snapshot);
    dispatch(&snapshot, CONTROL_EVENT_SYSTEM_FAULT, 0U);
    assert(snapshot.state == CONTROL_STATE_FAULT);
    assert(snapshot.fault == CONTROL_FAULT_EVENT_OVERFLOW);
    assert(snapshot.heater_enabled == 0U);
}

int main(void)
{
    test_door_open_stops_heating();
    test_overtemperature_latches_fault();
    test_sensor_timeout_stops_heating();
    test_countdown_completion();
    test_update_request_is_safe();
    test_system_fault_is_latched();
    puts("control tests passed");
    return 0;
}
