# Target Core Layout

This directory is a scaffold for the target STM32/CubeIDE project layout.

The current repository intentionally keeps the imported firmware files in the
flat `firmware/` directory until the full embedded project is assembled. This
avoids accidental breakage from changing include paths before the CubeIDE
project, linker script, and generated driver files are present.

Use this layout as the migration target:

- `App/`
- `Safety/`
- `Protocol/`
- `Config/`
- `Control/`
- `Drivers/`
