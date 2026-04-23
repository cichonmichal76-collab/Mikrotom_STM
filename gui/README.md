# Mikrotom GUI

Static operator GUI for the STM32 + Agent + GUI + HMI architecture.

## Scope

The GUI is an operator-facing layer. It:

- shows axis state, position, velocity, current, and fault state
- supports commissioning stage workflow
- exposes safety-related settings and conservative motion controls
- visualizes motion, soft limits, and recent event history
- provides a raw service command panel

The GUI does not talk to STM32 directly. It expects a local agent/backend that
translates REST API calls into UART protocol messages for the firmware.

## Modes

- `demoMode: true` in `config.js` runs the app without a backend
- `demoMode: false` enables live REST API calls

## Expected REST API

### GET

- `/api/status`
- `/api/telemetry/latest`
- `/api/events/recent`

### POST

- `/api/cmd/enable`
- `/api/cmd/disable`
- `/api/cmd/stop`
- `/api/cmd/qstop`
- `/api/cmd/move-rel`
- `/api/cmd/move-abs`
- `/api/cmd/raw`
- `/api/params`

## Expected JSON shapes

### `GET /api/status`

```json
{
  "axis_state": "SAFE",
  "fault": "NONE",
  "fault_mask": 0,
  "commissioning_stage": 1,
  "safe_mode": 1,
  "arming_only": 0,
  "controlled_motion": 0,
  "run_allowed": 0,
  "position_um": 0,
  "position_set_um": 0,
  "brake_installed": 0,
  "ignore_brake_feedback": 0,
  "calib_valid": 0,
  "max_current": 0.2,
  "max_velocity": 0.005,
  "soft_min_pos": -10000,
  "soft_max_pos": 10000
}
```

### `GET /api/telemetry/latest`

```json
{
  "ts_ms": 0,
  "pos_um": 0,
  "pos_set_um": 0,
  "vel_um_s": 0,
  "vel_set_um_s": 0,
  "iq_ref_mA": 0,
  "iq_meas_mA": 0,
  "state": 1,
  "fault": 0
}
```

### `GET /api/events/recent`

```json
[
  {
    "ts_ms": 0,
    "code": "BOOT",
    "value": 0
  }
]
```

### `POST /api/params`

The GUI sends a normalized JSON payload. Typical fields:

```json
{
  "commissioning_stage": 3,
  "brake_installed": 1,
  "ignore_brake_feedback": 0,
  "max_current": 0.2,
  "max_velocity": 0.005,
  "soft_min_pos": -10000,
  "soft_max_pos": 10000
}
```

The agent should translate this into the UART protocol supported by the
firmware, for example:

- `SET CONFIG COMMISSION_STAGE 3`
- `SET CONFIG BRAKE_INSTALLED 1`
- `SET CONFIG IGNORE_BRAKE_FEEDBACK 0`
- `SET PARAM MAX_CURRENT 0.2`
- `SET PARAM MAX_VELOCITY 0.005`
- `SET PARAM SOFT_MIN_POS -10000`
- `SET PARAM SOFT_MAX_POS 10000`

## Running locally

Serve the `gui/` directory with any static web server, for example:

```bash
python -m http.server 8080
```

Then open:

`http://127.0.0.1:8080/gui/`
