# HMI Packet Specification

## Purpose

This document defines the machine-side packet contract for integrating an HMI
panel with the Mikrotom STM32 control system.

It does **not** describe the vendor-internal BuyDisplay UI project file format
or the UI Editor upload process. It defines the packets that the machine
controller stack understands on the control side.

## Scope

This specification applies to:

- direct `HMI <-> STM32` UART integration
- development proxy mode `HMI <-> PC agent <-> STM32`
- service tools that need the same command grammar

The protocol is intentionally aligned with the existing STM32 firmware command
set.

## Recommended topologies

### Target runtime

```text
HMI <-> dedicated STM32 UART
PC  <-> service UART / agent
```

This is the preferred production architecture.

### Development proxy mode

```text
HMI <-> PC COM-B
MCU <-> PC COM-A
PC app / agent bridges both sides
```

This is useful during bring-up when the HMI and MCU are both connected through
independent channels of a multi-UART USB adapter.

## Electrical layer

- UART TTL, point-to-point
- default link settings: `115200`, `8N1`
- one owner per UART channel
- common `GND` is mandatory
- HMI power should be provided by a proper `5V` supply, not from a weak UART
  adapter `VCC`

## Framing

- encoding: ASCII / UTF-8 compatible text
- one request or response per line
- line terminator: `\n`
- commands are case-sensitive
- tokens are separated by a single ASCII space

Examples:

```text
GET STATE
SET PARAM MAX_CURRENT 0.200000
CMD MOVE_REL 100
```

## Roles

### HMI or host side

The HMI host may:

- read state
- read telemetry snapshots
- read recent events
- send operator commands
- send configuration updates

The HMI host must not:

- assume motion is allowed without checking `RUN_ALLOWED`
- assume the build supports motion without checking `MOTION_IMPLEMENTED`
- bypass `SAFE`, `FAULT`, or commissioning gates

### STM32 side

STM32 remains the final authority for:

- motion permission
- fault handling
- commissioning stage gating
- safety override acceptance

## Request packets

### 1. Read simple runtime values

Format:

```text
GET <NAME>
```

Supported names in the current firmware:

- `STATE`
- `FAULT`
- `FAULT_MASK`
- `COMMISSION_STAGE`
- `SAFE_MODE`
- `ARMING_ONLY`
- `CONTROLLED_MOTION`
- `RUN_ALLOWED`
- `ENABLED`
- `CONFIG_LOADED`
- `SAFE_INTEGRATION`
- `MOTION_IMPLEMENTED`
- `POS`
- `EVENT_COUNT`
- `EVENT_POP`
- `VERSION`

Examples:

```text
GET STATE
GET RUN_ALLOWED
GET CONFIG_LOADED
GET VERSION
```

### 2. Read parameter values

Format:

```text
GET PARAM <NAME>
```

Current parameter names:

- `MAX_CURRENT`
- `MAX_CURRENT_PEAK`
- `MAX_VELOCITY`
- `MAX_ACCELERATION`
- `SOFT_MIN_POS`
- `SOFT_MAX_POS`
- `CALIB_ZERO_POS`
- `CALIB_PITCH_UM`
- `CALIB_SIGN`

### 3. Read configuration values

Format:

```text
GET CONFIG <NAME>
```

Current configuration names:

- `BRAKE_INSTALLED`
- `COLLISION_SENSOR_INSTALLED`
- `PTC_INSTALLED`
- `BACKUP_SUPPLY_INSTALLED`
- `EXTERNAL_INTERLOCK_INSTALLED`
- `IGNORE_BRAKE_FEEDBACK`
- `IGNORE_COLLISION_SENSOR`
- `IGNORE_EXTERNAL_INTERLOCK`
- `ALLOW_MOTION_WITHOUT_CALIBRATION`
- `CALIB_VALID`

### 4. Set parameter values

Format:

```text
SET PARAM <NAME> <VALUE>
```

Examples:

```text
SET PARAM MAX_CURRENT 0.200000
SET PARAM MAX_CURRENT_PEAK 0.300000
SET PARAM MAX_VELOCITY 0.005000
SET PARAM SOFT_MIN_POS -10000
SET PARAM SOFT_MAX_POS 10000
```

### 5. Set configuration values

Format:

```text
SET CONFIG <NAME> <VALUE>
```

Examples:

```text
SET CONFIG COMMISSION_STAGE 3
SET CONFIG BRAKE_INSTALLED 1
SET CONFIG IGNORE_BRAKE_FEEDBACK 0
SET CONFIG ALLOW_MOTION_WITHOUT_CALIBRATION 0
```

### 6. Operator commands

Format:

```text
CMD <VERB> [ARG]
```

Current verbs:

- `HEARTBEAT`
- `ENABLE`
- `DISABLE`
- `STOP`
- `QSTOP`
- `ACK_FAULT`
- `CALIB_ZERO`
- `MOVE_REL <delta_um>`
- `MOVE_ABS <target_um>`

Examples:

```text
CMD HEARTBEAT
CMD ENABLE
CMD STOP
CMD QSTOP
CMD ACK_FAULT
CMD CALIB_ZERO
CMD MOVE_REL 100
CMD MOVE_ABS 2500
```

## Response packets

### Generic OK

```text
RSP,OK
```

### Generic error

```text
RSP,ERR,<REASON>
```

Examples:

```text
RSP,ERR,MOVE_REL_REJECTED
RSP,ERR,SAVE_FAILED
RSP,ERR,FAULT_ACTIVE
```

### Runtime state

```text
RSP,STATE,<STATE_NAME>
```

Examples:

```text
RSP,STATE,SAFE
RSP,STATE,CONFIG
RSP,STATE,READY
RSP,STATE,MOTION
RSP,STATE,FAULT
```

### Fault state

```text
RSP,FAULT,<FAULT_NAME>
```

### Parameter response

```text
RSP,PARAM,<NAME>,<VALUE>
```

### Configuration response

```text
RSP,CONFIG,<NAME>,<VALUE>
```

### Firmware version response

```text
RSP,VERSION,<NAME>,<VERSION>,<BUILD_DATE>,<GIT_HASH>
```

### Event response

One event:

```text
RSP,EVENT,<TS_MS>,<CODE>,<VALUE>
```

No event:

```text
RSP,EVENT,NONE
```

## Asynchronous telemetry packets

Format:

```text
TEL,<TS_MS>,<POS_UM>,<POS_SET_UM>,<VEL_UM_S>,<VEL_SET_UM_S>,<IQ_REF_MA>,<IQ_MEAS_MA>,<STATE>,<FAULT>
```

Example:

```text
TEL,18452,1200,1500,320,0,180,175,6,0
```

Field meanings:

- `TS_MS`: MCU uptime in milliseconds
- `POS_UM`: actual position
- `POS_SET_UM`: position setpoint
- `VEL_UM_S`: actual velocity
- `VEL_SET_UM_S`: velocity setpoint
- `IQ_REF_MA`: current reference
- `IQ_MEAS_MA`: measured current
- `STATE`: numeric state code from firmware
- `FAULT`: numeric fault code from firmware

## HMI polling and refresh guidance

Recommended polling for a UART HMI host:

- `GET STATE`, `GET FAULT`, `GET RUN_ALLOWED`: every `100-250 ms`
- `GET CONFIG_LOADED`, `GET MOTION_IMPLEMENTED`: every `500-1000 ms`
- `GET PARAM/CONFIG` values: on page entry or after a save
- `GET EVENT_COUNT` + `GET EVENT_POP`: every `250-1000 ms`

If `TEL,...` is available to the HMI host, prefer using it for live indicators
instead of polling positional values separately.

## HMI screen action mapping

Recommended mapping from HMI buttons/widgets to packets:

- Enable button -> `CMD ENABLE`
- Disable button -> `CMD DISABLE`
- Stop button -> `CMD STOP`
- QStop button -> `CMD QSTOP`
- Ack fault button -> `CMD ACK_FAULT`
- Calibrate zero button -> `CMD CALIB_ZERO`
- Move relative widget -> `CMD MOVE_REL <delta_um>`
- Move absolute widget -> `CMD MOVE_ABS <target_um>`
- Commissioning stage selector -> `SET CONFIG COMMISSION_STAGE <1|2|3>`
- Safety checkboxes -> `SET CONFIG ...`
- Limit editors -> `SET PARAM ...`

## HMI safety rules

The HMI must follow these rules:

- disable movement widgets when `RUN_ALLOWED != 1`
- also disable movement widgets when `MOTION_IMPLEMENTED != 1`
- keep `STOP` and `QSTOP` always visible
- show explicit warning when `STATE=CONFIG`
- show fault acknowledge only when a fault is active
- treat `SAFE_INTEGRATION=1` as a commissioning / no-motion build

## Development notes for BuyDisplay smart HMI

For BuyDisplay smart UART HMI panels:

- the HMI project itself is built with the vendor UI Editor tooling
- project upload is vendor-specific and outside this packet spec
- the machine-side application should still use the packet set above when
  talking to STM32 or to a PC-side bridge

This separation keeps the machine protocol stable even if the HMI vendor or UI
project changes.
