# FreeRTOS Heating Device Fault-Safe Controller

STM32F103C8 firmware for a heating device controller. The project focuses on
safe actuator control under asynchronous input instead of a simple LCD and key
demo.

## Design

`control_task` is the only task that owns `ControlSnapshot`. Key interrupts,
the one-second timer and the DHT11 worker submit typed events to its bounded
queue. `actuator.c` is the only application module that writes the door, heater,
turntable and countdown-timer registers.

## Hardware Contract

| Signal | Pin | Behavior |
| --- | --- | --- |
| Door interlock | PB12 / EXTI12 | Active-low physical input. Both edges are observed; an open event zeros heater and turntable PWM in the ISR before queueing the state event. |
| Start key | PB10 / EXTI10 | Active-low request to start heating. |
| Mode key | PB1 / EXTI1 | Active-low short/long press for time and power configuration. |
| Heater | PA2 / TIM2_CH3 | PWM output, written only by `actuator.c`. |
| Door servo | PA1 / TIM2_CH2 | Door actuator output. |
| Turntable | PB0 / TIM3_CH3 | PWM output, written only by `actuator.c`. |
| Temperature | PC14 | DHT11 data pin. |
| Upgrade staging | W25Q64 on SPI1, CS PA4 | Shares PA5/PA6/PA7 with the LCD under an RTOS mutex. |

The watchdog uses the independent IWDG. It is reloaded only while the control
task continues to make progress; a stalled control task first receives a safe
PWM shutdown, then is reset by IWDG.

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

## Upgrade Contract

UART1 receives firmware through DMA1 Channel 5 in circular mode. Half-transfer,
transfer-complete and IDLE interrupts drain the RX ring into the lower-priority
upgrade worker, keeping Flash I/O out of interrupt context.

Frame format is little-endian:

```
A5 5A | type:u8 | payload_length:u16 | payload | crc16_ccitt:u16
```

- `BEGIN` (`0x01`): 12-byte payload of image size, image CRC32 and version.
- `DATA` (`0x02`): sequence number followed by up to 238 bytes of image data.
- `FINISH` (`0x03`): zero-length payload; accepted only when all bytes and CRC32 match.
- `ABORT` (`0x04`): discards the in-memory session.

Verified data is staged in W25Q64. A CRC-protected `BootManifest` is then
written to the dedicated final Flash sector. Before publishing that manifest,
the application reads the staged image back in blocks and recalculates CRC32.
Only after `control_task` has
applied its update-safe state does the application set the backup-register
mailbox and reset. The transactional Bootloader consumes and clears that
mailbox, verifies the manifest and installs or rolls back the staged image.

`USER/heatguard_config.h` is the single product-configuration boundary for
queue depth, key timing, sensor period, maximum cooking time and fault
thresholds. `USER/diagnostics.c` maintains the debugger-visible global
`g_app_diagnostics`, including heap headroom, control-task stack watermark,
queue headroom and fault state. It avoids allocating diagnostic strings or
printing from control paths.

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
The handoff test covers the standard CRC32 vector and rejects a corrupted
Bootloader manifest.

GitHub Actions runs this same host test on every push and pull request.

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
