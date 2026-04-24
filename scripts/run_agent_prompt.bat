@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

set "PORT=%~1"

if "%PORT%"=="" (
    echo === Mikrotom Agent launcher ===
    echo Enter COM port used by the USB-UART adapter.
    echo Example: COM3
    echo.
    set /p PORT=COM port: 
)

if "%PORT%"=="" (
    set "PORT=COM3"
)

call "%ROOT%\scripts\run_agent.bat" "%PORT%"
exit /b %ERRORLEVEL%
