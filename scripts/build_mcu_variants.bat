@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

set "VARIANT=%~1"
if "%VARIANT%"=="" set "VARIANT=both"

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\build_mcu_variants.ps1" -Variant "%VARIANT%"
exit /b %ERRORLEVEL%
