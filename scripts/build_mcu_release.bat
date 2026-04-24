@echo off
setlocal EnableExtensions

echo build_mcu_release.bat now builds the SAFE variant explicitly.
echo Use scripts\build_mcu_variants.bat both for SAFE + MOTION-ENABLED.
echo.

call "%~dp0build_mcu_variants.bat" safe
exit /b %ERRORLEVEL%
