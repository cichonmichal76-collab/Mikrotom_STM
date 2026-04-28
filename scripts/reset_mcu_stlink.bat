@echo off
setlocal

set "PROGRAMMER=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe"

if not exist "%PROGRAMMER%" (
    echo ERROR: STM32_Programmer_CLI.exe not found:
    echo %PROGRAMMER%
    exit /b 1
)

"%PROGRAMMER%" -c port=SWD mode=UR -rst
if errorlevel 1 (
    echo.
    echo ERROR: MCU reset failed.
    exit /b %errorlevel%
)

echo.
echo MCU reset complete.
exit /b 0
