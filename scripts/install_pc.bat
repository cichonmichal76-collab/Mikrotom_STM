@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

echo === Mikrotom PC install ===
echo Project: %ROOT%

where py >nul 2>nul
if %ERRORLEVEL%==0 (
    set "PY=py -3"
) else (
    where python >nul 2>nul
    if not %ERRORLEVEL%==0 (
        echo ERROR: Python was not found. Install Python 3.12+ and retry.
        exit /b 1
    )
    set "PY=python"
)

if not exist ".venv\Scripts\python.exe" (
    echo Creating .venv ...
    %PY% -m venv .venv
    if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%
)

echo Installing agent dependencies ...
".venv\Scripts\python.exe" -m pip install --upgrade pip
if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%
".venv\Scripts\python.exe" -m pip install -r agent\requirements.txt
if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%

echo.
echo PC install complete.
echo.
echo Next steps:
echo   scripts\configure_gui_live.bat
echo   scripts\run_agent.bat COM3
echo   scripts\run_gui.bat
echo.
echo Use scripts\configure_gui_demo.bat to return GUI to demo mode.
