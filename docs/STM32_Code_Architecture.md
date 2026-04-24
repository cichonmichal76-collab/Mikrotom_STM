# STM32 Code Architecture

## Purpose

This document defines the target firmware architecture for the STM32 + Agent +
GUI system. It covers:

- firmware module structure
- dependencies between layers
- communication interfaces
- implementation rules
- migration guidance from the current integrated package

The machine-side HMI packet contract is specified separately in
`docs/HMI_Protocol.md`.

## System view

```text
GUI -> Agent -> UART -> STM32 -> FOC -> Hardware
```

## Logical layers

```text
Application Layer
    -> Control Layer (AxisControl)
        -> Safety Layer (Fault, Watchdog, Limits, SafetyMonitor)
            -> Communication Layer (Protocol, Telemetry, EventLog)
                -> Hardware Control (FOC, PWM, ADC, Timers)
```

## Target STM32 project layout

```text
Core/
 ├── App/
 │    ├── axis_state.*
 │    ├── axis_control.*
 │    ├── commissioning.*
 │
 ├── Safety/
 │    ├── fault.*
 │    ├── watchdogs.*
 │    ├── limits.*
 │    ├── safety_config.*
 │
 ├── Protocol/
 │    ├── protocol.*
 │    ├── telemetry.*
 │    ├── eventlog.*
 │
 ├── Config/
 │    ├── config_store.*
 │    ├── calibration.*
 │    ├── fw_version.*
 │
 ├── Drivers/      (CubeMX-generated and MCU-specific)
 │    ├── adc.*
 │    ├── tim.*
 │    ├── usart.*
 │
 ├── Control/
 │    ├── foc.*
 │    ├── pid.*
 │    ├── trajectory.*
 │    ├── motorState.*
```

## Current repository mapping

The repository currently keeps imported firmware in a flat `firmware/`
directory to avoid breaking safe integration before the full CubeIDE project is
assembled.

The importable STM32CubeIDE build target now lives in `stm32_cubeide/`. It is
based on the original working project and mirrors the current `firmware/*.c`
and `firmware/*.h` files into `Core/Src` and `Core/Inc`.

Current file ownership by target layer:

- `App`: `axis_state.*`, `axis_control.*`, `commissioning.*`
- `Safety`: `fault.*`, `watchdogs.*`, `limits.*`, `safety_config.*`,
  `safety_monitor.*`
- `Protocol`: `protocol.*`, `telemetry.*`, `eventlog.*`
- `Config`: `config_store.*`, `calibration.*`, `fw_version.*`
- `Control`: `motorState.*` plus external `foc.*`, `pid.*`, `trajectory.*`
- `Integration`: `main.c`, `stm32g4xx_it.c`, `app_build_config.h`

## Central runtime data

### MotorState

`MotorState` is the central runtime structure for position, velocity, current,
and motion enable status. The project keeps the existing extended structure
because it still carries hardware-specific current and encoder fields needed by
the original FOC implementation.

Conceptually it must include:

```c
typedef struct {
    float pos_um;
    float pos_m;
    float vel_m_s;

    float pos_set_m;
    float vel_set_m_s;

    float iq_ref;
    float iq_meas;

    float maxcurrent;
    float Vbus;

    uint8_t enabled;
    uint8_t calibrated;
} MotorState;
```

### MotorTrajectory

```c
typedef struct {
    float start_pos_m;
    float dest_pos_m;
    float pos_set_m;
    float vel_set_m_s;

    uint8_t traj_done;
} MotorTrajectory;
```

## Module responsibilities

### App layer

#### `axis_state`

Responsibility:

- own the global axis state machine

Primary API:

- `AxisState_Set(...)`
- `AxisState_Get(...)`
- `AxisState_CanMove(...)`

#### `axis_control`

Responsibility:

- execute operator-level commands
- validate motion conditions
- bridge application logic to control logic

Primary API:

- `AxisControl_Enable()`
- `AxisControl_MoveRelUm()`
- `AxisControl_MoveAbsUm()`
- `AxisControl_RunAllowed()`
- `AxisControl_UpdateRt()`

#### `commissioning`

Responsibility:

- manage Stage 1 / 2 / 3
- expose commissioning gates to the rest of the firmware

Primary API:

- `Commissioning_SetStage(...)`
- `Commissioning_RunAllowed(...)`

### Safety layer

#### `fault`

Responsibility:

- own fault mask and fault reporting

Primary API:

- `Fault_Set(...)`
- `Fault_Clear(...)`
- `Fault_IsActive(...)`

#### `watchdogs`

Responsibility:

- detect communication loss
- raise deterministic faults

#### `limits`

Responsibility:

- constrain current, velocity, and position

Primary API:

- `Limits_ClampCurrentA(...)`
- `Limits_ClampVelocity(...)`
- `Limits_IsMoveRelAllowed(...)`
- `Limits_IsMoveAbsAllowed(...)`

#### `safety_config`

Responsibility:

- hold configurable safety flags and conscious overrides

#### `safety_monitor`

Responsibility:

- supervise runtime electrical and motion conditions before FOC execution
- raise deterministic faults for overcurrent, undervoltage, overvoltage,
  overspeed, tracking error, and post-homing soft-limit violations
- debounce fast signals so one noisy sample does not immediately latch a fault

Primary API:

- `SafetyMonitor_Init()`
- `SafetyMonitor_UpdateRt(...)`

### Protocol layer

#### `protocol`

Responsibility:

- own UART command parsing and command dispatch

Supported command style:

```text
GET STATE
SET PARAM MAX_CURRENT 0.3
CMD MOVE_REL 100
```

#### `telemetry`

Responsibility:

- collect and flush runtime samples toward the agent

Primary API:

- `Telemetry_Sample(...)`
- `Telemetry_Flush()`

#### `eventlog`

Responsibility:

- maintain recent structured event history

### Config layer

#### `config_store`

Responsibility:

- load and save configuration, calibration, and safety settings

#### `calibration`

Responsibility:

- hold electrical zero, pitch, direction, and validity state

#### `fw_version`

Responsibility:

- expose firmware identity/version/build metadata

### Control layer

#### `motorState`

Responsibility:

- hardware-facing runtime state update
- current sampling and encoder-derived position update
- VBUS ADC sampling and conversion to motor supply voltage

#### `foc`, `pid`, `trajectory`

Responsibility:

- existing control implementation
- must remain stable during the safety/protocol migration

## Data flow

### Control path

```text
GUI -> Agent/API -> Protocol -> AxisControl -> FOC -> Motor
```

### Telemetry path

```text
STM32 -> Telemetry -> UART -> Agent -> GUI
```

### Runtime status path

```text
STM32 -> Protocol GET values -> Agent /api/status -> GUI
```

`VBUS` is intentionally exposed through the status path instead of extending the
asynchronous `TEL,...` line. This keeps the real-time telemetry packet stable
while still making supply-voltage supervision visible to the operator.
`VBUS_VALID` indicates whether at least one ADC conversion has populated the
value. Motion must remain inhibited while this flag is false.

## Main loop

The firmware main loop should remain minimal and deterministic:

```c
while (1)
{
    Protocol_Process();
    Telemetry_Flush();
    Watchdogs_Update();
    AxisControl_BackgroundTask();
}
```

## Real-time callback order

The ADC-injected conversion callback remains the main real-time path:

```text
1. current measurement and VBUS state already latched from ADC callbacks
2. position and velocity update
3. SafetyMonitor_UpdateRt()
4. immediate output inhibit if a fault is active
5. AxisControl_UpdateRt()
6. Telemetry_Sample()
7. FOC only when RUN_ALLOWED is true and outputs are not inhibited
```

## Implementation rules

### ISR rules

- keep ISRs minimal
- no `printf`
- no blocking logic
- no business workflow logic inside ISR paths

### Blocking rules

Do not use:

- `HAL_Delay()` in runtime control paths
- busy waits such as `while(flag == 0)`

### Memory rules

- no `malloc`
- static buffers only

### Global state rules

- keep globals limited
- prefer `static`
- expose state only through module APIs where possible

### Interface rules

Each module should expose a small API shaped as:

```c
module_function(...)
```

## Integration policy

### Do not change during migration

- FOC core
- PID core
- ADC timing assumptions
- PWM generation path

### Add during migration

- safety layer
- UART protocol layer
- telemetry and event reporting

### Disable during migration

- legacy UART streaming that conflicts with the new protocol
- automatic startup movement
- any behavior that can surprise the operator

## RUN_ALLOWED

Motion must always be guarded by:

```c
if (!AxisControl_RunAllowed())
{
    /* motion blocked */
}
```

The intended conditions are:

- no active faults
- VBUS sampled and inside the valid operating window
- commissioning stage 3
- `safe_mode == 0`
- calibration valid
- motion explicitly enabled

## Test sequence

### Stage 1

- no motion
- communication verified
- faults and watchdogs verified
- `GET VBUS_VALID` becomes `1`
- `GET VBUS` returns a plausible supply voltage or a safe known value

### Stage 2

- commissioning logic verified
- enable/disable path verified
- STOP, QSTOP, fault, and disable paths force output inhibit

### Stage 3

- first controlled motion
- conservative limits
- operator supervision with STOP/QSTOP ready
- runtime faults verified with safe thresholds or controlled simulation

The detailed bring-up checklist is maintained in
`docs/STM32_Bringup_Checklist.md`.

## Common failure modes

| Problem | Typical cause |
| --- | --- |
| no motion | `RUN_ALLOWED == 0` |
| strange data | broken telemetry mapping |
| UART conflict | two concurrent stream producers |
| crash / instability | ISR doing too much work |

## Key rule

The firmware must never surprise the operator.
