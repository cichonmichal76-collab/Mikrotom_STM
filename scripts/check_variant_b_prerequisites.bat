@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

echo === Mikrotom Variant B - prerequisite check ===
echo Project: %ROOT%
echo.

set "HAS_PYTHON=0"
where py >nul 2>nul
if %ERRORLEVEL%==0 (
    echo [OK] Python launcher: py
    set "HAS_PYTHON=1"
) else (
    where python >nul 2>nul
    if %ERRORLEVEL%==0 (
        echo [OK] Python executable: python
        set "HAS_PYTHON=1"
    ) else (
        echo [MISSING] Python 3.12+ was not found in PATH.
    )
)

set "STM32_PROGRAMMER_CLI=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
if not exist "%STM32_PROGRAMMER_CLI%" (
    set "STM32_PROGRAMMER_CLI=%ProgramFiles(x86)%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
)

if exist "%STM32_PROGRAMMER_CLI%" (
    echo [OK] STM32CubeProgrammer CLI found:
    echo      %STM32_PROGRAMMER_CLI%
) else (
    echo [MISSING] STM32CubeProgrammer CLI was not found.
)

if exist "%ROOT%\dist\mcu\Mikrotom_STM_latest.hex" (
    echo [OK] MCU firmware file found.
) else (
    echo [MISSING] dist\mcu\Mikrotom_STM_latest.hex
)

if exist "%ROOT%\docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md" (
    echo [OK] Deployment documentation found.
) else (
    echo [MISSING] Deployment documentation file.
)

echo.
echo Notes:
echo - USB-UART COM port is checked in Windows Device Manager.
echo - ST-LINK is required only for flashing MCU by SWD.
echo - Current firmware package is SAFE INTEGRATION and should not move the axis.
echo.

if "%HAS_PYTHON%"=="0" (
    echo ACTION: Install Python 3.12+ first, then run 02_INSTALUJ_PC.bat.
)

if not exist "%STM32_PROGRAMMER_CLI%" (
    echo ACTION: Install STM32CubeProgrammer before flashing MCU.
)

exit /b 0
