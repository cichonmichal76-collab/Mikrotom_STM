@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if /I "%~1"=="motion" (
    set "FW=%ROOT%\dist\mcu\Mikrotom_STM_latest_motion.hex"
) else if /I "%~1"=="safe" (
    set "FW=%ROOT%\dist\mcu\Mikrotom_STM_latest_safe.hex"
) else if "%~1"=="" (
    set "FW=%ROOT%\dist\mcu\Mikrotom_STM_latest_safe.hex"
) else (
    set "FW=%~1"
)

set "STM32_PROGRAMMER_CLI=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if not exist "%STM32_PROGRAMMER_CLI%" (
    set "STM32_PROGRAMMER_CLI=%ProgramFiles(x86)%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
)

if not exist "%STM32_PROGRAMMER_CLI%" (
    echo ERROR: STM32_Programmer_CLI.exe not found.
    echo Install STM32CubeProgrammer or add it to this script.
    exit /b 1
)

if not exist "%FW%" (
    echo ERROR: firmware file not found:
    echo   %FW%
    echo Run scripts\build_mcu_variants.bat both first.
    exit /b 1
)

echo === MCU FLASH WARNING ===
echo Firmware: %FW%
echo.
echo Recommended order:
echo   1. Flash SAFE first: scripts\flash_mcu_stlink.bat safe
echo   2. Validate communication, telemetry and safety
echo   3. Flash MOTION only after Stage 1/2 are confirmed
echo.
echo If you flash MOTION:
echo   - keep STOP/QSTOP and supply cutoff ready
echo   - start with conservative limits
echo   - execute HOME first, then a tiny MOVE_REL
echo.
pause

"%STM32_PROGRAMMER_CLI%" -c port=SWD -w "%FW%" -v -rst
