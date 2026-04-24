# STM32 CubeIDE Build Target

## Purpose

The repository now contains a full STM32CubeIDE project under
`stm32_cubeide/`. It was copied from the original working controller project
and then synchronized with the current firmware sources.

This keeps the hardware configuration 1:1 with the known-good baseline while
making the repository self-contained for MCU builds.

## What Was Copied

From the original project:

- `.project`, `.cproject`, `.mxproject`, `.settings`
- `.ioc`
- `Core/`
- `Drivers/`
- linker script
- debug launch configuration

Not copied:

- `Debug/`
- `Release/`
- generated object files and build outputs

Those generated folders are ignored by `.gitignore`.

## What Was Synchronized

The current `firmware/*.c` files were copied into:

```text
stm32_cubeide/Core/Src
```

The current `firmware/*.h` files were copied into:

```text
stm32_cubeide/Core/Inc
```

This adds the new system modules to the CubeIDE build target, including:

- `axis_control`
- `fault`
- `limits`
- `protocol`
- `telemetry`
- `config_store`
- `safety_monitor`

The original hardware/control files that are not present in `firmware/`, such
as `foc.*`, `pid.*`, `trajectory.*`, `focMath.*`, and generated HAL files,
remain from the original CubeIDE project.

## Import In STM32CubeIDE

1. Open STM32CubeIDE.
2. Choose `File -> Import -> Existing Projects into Workspace`.
3. Select the repository folder:

```text
C:\Users\michal.cichon\Desktop\Mikrotom_STM\stm32_cubeide
```

4. Import the detected project.
5. Build the `Debug` configuration first.

The CubeIDE project metadata intentionally preserves the original CubeMX project
identity. If the same original project is already imported in the workspace,
use a clean workspace or rename the imported project inside CubeIDE after the
first successful import.

## First Build Expectations

The first build should verify:

- all new `.c` files are compiled
- `safety_monitor.c` is included in the build
- no old debug UART stream conflicts with protocol/telemetry
- `APP_SAFE_INTEGRATION=1` remains the default no-motion build

## Verified Local Build

The project was built locally with STM32CubeIDE 1.19.0 tooling installed under:

```text
C:\ST\STM32CubeIDE_1.19.0
```

After CubeIDE generated `stm32_cubeide/Debug`, the generated makefile was built
directly with the bundled `make.exe` and `arm-none-eabi-gcc.exe`.

Build result:

```text
text    data    bss     dec     hex
63456   500     8504    72460   11b0c
```

Output artifact:

```text
stm32_cubeide/Debug/SterownikImpulsowySilnika_109-B-G431B-ESC1.elf
```

`Debug/` remains ignored by git because it is generated build output.

## Headless Build Notes

If STM32CubeIDE is installed outside `PATH`, use the bundled tools directly:

```powershell
$makeDir = 'C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin'
$gccDir = 'C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin'
$env:Path = "$makeDir;$gccDir;$env:Path"
& "$makeDir\make.exe" -C stm32_cubeide\Debug -j8 all
```

## Important Safety Note

Do not regenerate code from CubeMX before the first successful build review.
CubeMX regeneration can overwrite `main.c`, `stm32g4xx_it.c`, and generated
sections. If regeneration is needed later, commit or back up the current state
first and review the generated diff carefully.
