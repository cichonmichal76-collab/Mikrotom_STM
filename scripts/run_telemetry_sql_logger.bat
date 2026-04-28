@echo off
setlocal
cd /d "%~dp0.."

if not exist ".venv\Scripts\python.exe" (
  echo Creating local Python venv...
  python -m venv .venv
)

".venv\Scripts\python.exe" -m pip install -r requirements-pc.txt
".venv\Scripts\python.exe" tools\telemetry_sql_logger.py --port COM6 --baud 460800 --db telemetry\mikrotom_telemetry.sqlite3 --echo

endlocal
