@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if exist ".venv\Scripts\python.exe" (
    set "PYEXE=%ROOT%\.venv\Scripts\python.exe"
) else (
    where py >nul 2>nul
    if %ERRORLEVEL%==0 (
        set "PYEXE=py -3"
    ) else (
        set "PYEXE=python"
    )
)

set "GUI_URL=http://127.0.0.1:8080/gui/"

echo === Mikrotom GUI ===
echo URL: %GUI_URL%
echo.
start "" "%GUI_URL%"

%PYEXE% -m http.server 8080 --bind 127.0.0.1 --directory "%ROOT%"
