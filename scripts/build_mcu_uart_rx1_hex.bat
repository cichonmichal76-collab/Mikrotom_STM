@echo off
setlocal

set "ROOT=%~dp0.."
set "MAKE_BIN=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin"
set "GCC_BIN=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
set "DEBUG_DIR=%ROOT%\Debug"
set "ELF=SterownikImpulsowySilnika_109-B-G431B-ESC1.elf"
set "HEX=SterownikImpulsowySilnika_109-B-G431B-ESC1_uart_rx1_115200.hex"

if not exist "%MAKE_BIN%\make.exe" (
    echo ERROR: make.exe not found:
    echo %MAKE_BIN%\make.exe
    exit /b 1
)

if not exist "%GCC_BIN%\arm-none-eabi-objcopy.exe" (
    echo ERROR: arm-none-eabi-objcopy.exe not found:
    echo %GCC_BIN%\arm-none-eabi-objcopy.exe
    exit /b 1
)

set "PATH=%MAKE_BIN%;%GCC_BIN%;%PATH%"

pushd "%DEBUG_DIR%"
make.exe -j12 all
if errorlevel 1 (
    popd
    echo.
    echo ERROR: Build failed.
    exit /b %errorlevel%
)

arm-none-eabi-objcopy.exe -O ihex "%ELF%" "%HEX%"
if errorlevel 1 (
    popd
    echo.
    echo ERROR: HEX generation failed.
    exit /b %errorlevel%
)

echo.
echo HEX generated:
echo %DEBUG_DIR%\%HEX%
popd
exit /b 0
