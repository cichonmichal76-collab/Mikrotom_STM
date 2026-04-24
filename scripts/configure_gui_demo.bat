@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

(
echo window.APP_CONFIG = {
echo   apiBaseUrl: "http://127.0.0.1:8000",
echo   refreshMs: 300,
echo   demoMode: true
echo };
) > "gui\config.js"

echo GUI configured for DEMO mode.
