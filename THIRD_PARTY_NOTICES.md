# Third-Party Notices

This repository includes components from the following upstream sources:

- FreeRTOS v9.0.0. Refer to the license headers in `FreeRTOS/`.
- STM32F10x Standard Peripheral Library. Refer to the license headers in
  `STM32F10x_FWLib/`.

The application-specific files are `USER/actuator.*`, `USER/control.*`,
`USER/diagnostics.*`, `USER/heatguard_config.h`, `USER/main.c`, the revised
short-delay and DHT11 drivers, plus `tests/`.

Legacy tutorial modules that are not part of the HeatGuardRT build are kept
locally for reference and excluded from version control.
