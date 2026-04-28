@echo off
setlocal

set "ROOT=%~dp0.."
set "PROGRAMMER=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe"
set "HEX=%ROOT%\Debug\SterownikImpulsowySilnika_109-B-G431B-ESC1_uart_rx1.hex"

if not exist "%PROGRAMMER%" (
    echo ERROR: STM32_Programmer_CLI.exe not found:
    echo %PROGRAMMER%
    exit /b 1
)

if not exist "%HEX%" (
    echo ERROR: HEX file not found:
    echo %HEX%
    echo Run scripts\build_mcu_uart_rx1_hex.bat first.
    exit /b 1
)

echo Programming MCU with:
echo %HEX%
echo.

"%PROGRAMMER%" -c port=SWD mode=UR -d "%HEX%" -v
if errorlevel 1 (
    echo.
    echo ERROR: MCU programming failed.
    exit /b %errorlevel%
)

echo.
echo Programming complete.
echo Run scripts\reset_mcu_stlink.bat after the stand is ready for firmware startup.
exit /b 0
