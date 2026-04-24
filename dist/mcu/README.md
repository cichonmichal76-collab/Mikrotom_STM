# MCU Firmware Release

Generated firmware files for STM32G431CBUx.

## Variants

### 1. SAFE integration

Files:

- `Mikrotom_STM_safe_integration_20260424.hex`
- `Mikrotom_STM_safe_integration_20260424.bin`
- `Mikrotom_STM_latest.hex`
- `Mikrotom_STM_latest.bin`
- `Mikrotom_STM_latest_safe.hex`
- `Mikrotom_STM_latest_safe.bin`

Build flags:

```c
APP_SAFE_INTEGRATION=1
APP_MOTION_IMPLEMENTED=0
```

What it does:

- full `GUI -> Agent -> UART -> MCU` path
- status, telemetry, event log and SQL path
- UART commands are accepted by the MCU
- no automatic motion after boot
- `MOVE_REL` / `MOVE_ABS` are intentionally rejected
- this is the recommended first flash

Expected first-boot behavior:

- no automatic motion
- protocol and telemetry available
- VBUS measurement available
- `SAFE_INTEGRATION = 1`
- `MOTION_IMPLEMENTED = 0`

### 2. MOTION enabled

Files:

- `Mikrotom_STM_motion_enabled_20260424.hex`
- `Mikrotom_STM_motion_enabled_20260424.bin`
- `Mikrotom_STM_latest_motion.hex`
- `Mikrotom_STM_latest_motion.bin`

Build flags:

```c
APP_SAFE_INTEGRATION=0
APP_MOTION_IMPLEMENTED=1
```

What it does:

- keeps the same communication, telemetry and safety path
- enables real motion execution
- requires explicit `CMD HOME` before the first controlled move
- should only be used after SAFE validation is complete

Expected first-boot behavior:

- still no automatic homing after boot
- protocol and telemetry available
- `SAFE_INTEGRATION = 0`
- `MOTION_IMPLEMENTED = 1`
- `CMD HOME` starts the homing sequence explicitly

## Flashing

Recommended order:

```bat
scripts\flash_mcu_safe_stlink.bat
scripts\flash_mcu_motion_stlink.bat
```

Or with the generic script:

```bat
scripts\flash_mcu_stlink.bat safe
scripts\flash_mcu_stlink.bat motion
```

The default generic script uses the SAFE variant.

## Verification After Flash

Check through UART / Agent:

### SAFE

- `GET SAFE_INTEGRATION` returns `1`
- `GET MOTION_IMPLEMENTED` returns `0`
- `GET VBUS_VALID` becomes `1`
- `GET FAULT` is `NONE`
- `MOVE_REL` / `MOVE_ABS` do not move the axis

### MOTION

- `GET SAFE_INTEGRATION` returns `0`
- `GET MOTION_IMPLEMENTED` returns `1`
- `GET HOMING_STARTED`, `GET HOMING_ONGOING`, `GET HOMING_SUCCESSFUL` report progress
- `CMD HOME` is accepted
- only after homing and all safety gates can `RUN_ALLOWED` become `1`
