@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if "%~1"=="" (
    set "FW=%ROOT%\dist\mcu\Mikrotom_STM_latest.hex"
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
    echo Run scripts\build_mcu_release.bat first.
    exit /b 1
)

echo === MCU FLASH WARNING ===
echo Firmware: %FW%
echo.
echo Recommended first flash:
echo   - APP_SAFE_INTEGRATION=1
echo   - power stage current-limited or disconnected
echo   - STOP/QSTOP and supply cutoff ready
echo.
pause

"%STM32_PROGRAMMER_CLI%" -c port=SWD -w "%FW%" -v -rst
