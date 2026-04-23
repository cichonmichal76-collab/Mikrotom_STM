# Mikrotom Agent

Minimal REST bridge between the static `gui/` frontend and the UART protocol
implemented by the STM32 firmware.

## Scope

The agent exposes:

- `GET /api/status`
- `GET /api/telemetry/latest`
- `GET /api/events/recent`
- `GET /api/debug/vars`
- `POST /api/params`

Command endpoints:

- `/api/cmd/enable`
- `/api/cmd/disable`
- `/api/cmd/ack-fault`
- `/api/cmd/stop`
- `/api/cmd/qstop`
- `/api/cmd/move-rel`
- `/api/cmd/move-abs`
- `/api/cmd/raw`

It translates those requests into the firmware protocol:

- `GET ...`
- `SET PARAM ...`
- `SET CONFIG ...`
- `CMD ...`

Telemetry lines (`TEL,...`) are parsed asynchronously from the same serial
stream. Recent events are drained from the firmware event log through:

- `GET EVENT_COUNT`
- `GET EVENT_POP`

The agent keeps its own short recent-events cache so the GUI can request
`/api/events/recent` repeatedly without needing direct event-log semantics.

The agent also persists MCU-facing logs to SQLite:

- protocol TX/RX lines
- telemetry samples
- event log entries

`/api/status` also surfaces the new program metadata needed by the GUI:

- runtime axis state and fault state
- commissioning flags and `RUN_ALLOWED`
- `enabled`, `config_loaded`, `safe_integration`, `motion_implemented`
- safety installation flags and override flags
- calibration validity and calibration parameters
- motion limits and setpoints

## Environment variables

- `MIKROTOM_SERIAL_PORT` default: `COM3`
- `MIKROTOM_SERIAL_BAUDRATE` default: `115200`
- `MIKROTOM_SERIAL_READ_TIMEOUT` default: `0.05`
- `MIKROTOM_COMMAND_TIMEOUT` default: `1.0`
- `MIKROTOM_HEARTBEAT_INTERVAL_S` default: `0.35`
- `MIKROTOM_EVENT_CACHE_SIZE` default: `128`
- `MIKROTOM_SQLITE_PATH` default: `agent/mikrotom_agent.sqlite3`

Set `MIKROTOM_SQLITE_PATH=off` to disable persistent logging.

## Install

```bash
python -m pip install -r agent/requirements.txt
```

For local tests:

```bash
python -m pip install -r requirements-dev.txt
```

## Run

```bash
python -m uvicorn agent.main:app --host 127.0.0.1 --port 8000
```

Then switch [gui/config.js](/C:/Users/cicho/OneDrive/Pulpit/Mikrotom_STM/gui/config.js:1)
to `demoMode: false`.

## Test

```bash
python -m pytest
```

## Debug endpoint

`GET /api/debug/vars` returns a wider service snapshot than the GUI uses:

- transport configuration and connection status
- firmware version info
- full status payload
- latest telemetry sample
- recent cached events
- SQLite log-store statistics
- exported field lists
