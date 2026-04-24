# MCU Firmware Release

Generated firmware files for STM32G431CBUx.

## Files

- `Mikrotom_STM_safe_integration_20260424.hex`
- `Mikrotom_STM_safe_integration_20260424.bin`
- `Mikrotom_STM_latest.hex`
- `Mikrotom_STM_latest.bin`

## Build Mode

This firmware is built from the default safe-integration configuration:

```c
APP_SAFE_INTEGRATION=1
APP_MOTION_IMPLEMENTED=0
```

Expected first-boot behavior:

- no automatic motion
- no real motion command execution
- protocol/telemetry path available
- VBUS measurement available
- `RUN_ALLOWED` remains blocked

## Flashing

With STM32CubeProgrammer installed:

```bat
scripts\flash_mcu_stlink.bat
```

The default flashing script uses:

```text
dist\mcu\Mikrotom_STM_latest.hex
```

## Verification After Flash

After flashing, check through UART/agent:

- `GET SAFE_INTEGRATION` returns `1`
- `GET MOTION_IMPLEMENTED` returns `0`
- `GET RUN_ALLOWED` returns `0`
- `GET VBUS_VALID` becomes `1`
- `GET FAULT` is `NONE`
