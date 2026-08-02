# FreeRTOS Heating Device Fault-Safe Controller

STM32F103C8 firmware for a heating device controller. The project focuses on
safe actuator control under asynchronous input instead of a simple LCD and key
demo.

## Design

`control_task` is the only task that owns `ControlSnapshot`. Key interrupts,
the one-second timer and the DHT11 worker submit typed events to its bounded
queue. `actuator.c` is the only application module that writes the door, heater,
turntable and countdown-timer registers.

- Key short press: toggle the door or configure time and power.
- Key long press: start heating only when the door is closed, a valid sensor
  sample exists, no fault is latched and time is non-zero.
- Door open: immediately revokes heating permission.
- Temperature at or above 30 C: latches an over-temperature fault and disables
  the heater and turntable.
- Three consecutive DHT11 failures while heating: latches a sensor timeout
  fault and disables the heater and turntable.
- Countdown expiry and update requests: disable the heater and turntable.
- Queue overflow: immediately zeros heater and turntable PWM, then latches an
  event-overflow fault in the control task.

The EXTI handlers contain no debounce delay and do not modify controller state.
They only notify the key workers. The key workers use `vTaskDelay()` while
sampling press duration, so they yield CPU time instead of busy waiting.

`SYSTEM/delay/delay.c` uses TIM1 as a 1 MHz short-delay source. SysTick is not
reprogrammed because it belongs to FreeRTOS after the scheduler starts.
Tickless Idle is disabled until peripheral wake-up behavior is verified on the
real board; the legacy low-power path disabled GPIO clocks used by the control
peripherals.

## Build

Use Keil MDK 5 with `USER/project.uvprojx`:

```powershell
& 'D:\Keil_v5\UV4\UV4.exe' -r 'USER\project.uvprojx'
```

The current build target is STM32F103C8 with 64 KiB Flash and 20 KiB RAM.
Legacy direct-write IAP, raw DMA receiver and direct Flash modules are excluded
from the target. USART RX DMA is intentionally not enabled until a bounded
transport, image metadata and CRC validation are integrated with the separate
transactional bootloader.

## Host Tests

The state machine has no STM32 or FreeRTOS dependency. Run its tests in WSL:

```bash
gcc -std=c99 -Wall -Wextra -Werror -IUSER USER/control.c tests/control_test.c -o /tmp/control_test
/tmp/control_test
```

The test covers door-open stop, over-temperature lockout, sensor timeout,
countdown completion, update-request safe stop and event-overflow lockout.

## Verification Boundary

Keil compilation and host state-transition tests are complete. The following
require the real circuit before they can be claimed as results: DHT11 signal
integrity, PWM output timing, door actuator behavior, end-to-end upgrade
transport and fault-to-output latency.

## Code Origin

The repository retains FreeRTOS, STM32 Standard Peripheral Library and legacy
peripheral drivers. The controller state machine, event scheduling, actuator
boundary, safety hooks, TIM1 delay implementation and host tests are
project-specific code.
