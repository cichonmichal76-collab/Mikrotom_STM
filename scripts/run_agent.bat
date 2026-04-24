@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if not exist ".venv\Scripts\python.exe" (
    echo Python environment missing. Running install_pc.bat first ...
    call "%ROOT%\scripts\install_pc.bat"
    if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%
)

if not "%~1"=="" (
    set "MIKROTOM_SERIAL_PORT=%~1"
)

if "%MIKROTOM_SERIAL_PORT%"=="" (
    set "MIKROTOM_SERIAL_PORT=COM3"
)

if "%MIKROTOM_SERIAL_BAUDRATE%"=="" (
    set "MIKROTOM_SERIAL_BAUDRATE=115200"
)

echo === Mikrotom Agent ===
echo Serial port: %MIKROTOM_SERIAL_PORT%
echo Baudrate:    %MIKROTOM_SERIAL_BAUDRATE%
echo API:         http://127.0.0.1:8000
echo.

".venv\Scripts\python.exe" -m uvicorn agent.main:app --host 127.0.0.1 --port 8000
