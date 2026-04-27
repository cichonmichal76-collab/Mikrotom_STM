# STM32CubeIDE Project

This folder is the MCU build target for the Mikrotom STM32 firmware.

It is based on the original working CubeIDE project and keeps the hardware
configuration, generated HAL files, linker script, startup code, and control
modules from that baseline.

Current firmware files from `../firmware/` have been synchronized into:

- `Core/Src`
- `Core/Inc`

	Build outputs such as `Debug/` and `Release/` are intentionally not tracked.

See `../docs/STM32_CubeIDE_Build.md` for import and build notes.

