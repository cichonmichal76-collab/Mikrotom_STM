@echo off
setlocal EnableExtensions

call "%~dp0flash_mcu_stlink.bat" safe
exit /b %ERRORLEVEL%
