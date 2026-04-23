# Mikrotom Agent

Minimal REST bridge between the static `gui/` frontend and the UART protocol
implemented by the STM32 firmware.

## Scope

The agent exposes:

- `GET /api/status`
- `GET /api/telemetry/latest`
- `GET /api/events/recent`
- `POST /api/cmd/*`
- `POST /api/params`

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

## Environment variables

- `MIKROTOM_SERIAL_PORT` default: `COM3`
- `MIKROTOM_SERIAL_BAUDRATE` default: `115200`
- `MIKROTOM_SERIAL_READ_TIMEOUT` default: `0.05`
- `MIKROTOM_COMMAND_TIMEOUT` default: `1.0`
- `MIKROTOM_EVENT_CACHE_SIZE` default: `128`

## Install

```bash
python -m pip install -r agent/requirements.txt
```

## Run

```bash
python -m uvicorn agent.main:app --host 127.0.0.1 --port 8000
```

Then switch [gui/config.js](/C:/Users/cicho/OneDrive/Pulpit/Mikrotom_STM/gui/config.js:1)
to `demoMode: false`.
