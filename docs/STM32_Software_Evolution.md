# STM32 Software Evolution

## Purpose

This document compares the original firmware shape with the expanded system that
adds safety, communication, commissioning, telemetry, event logging, and GUI
integration.

It explains:

- what the original software looked like
- what its practical limits were
- what new mechanisms were introduced
- how the architecture changed
- why the firmware is no longer just a motor driver

## Original version

### Original architecture

```text
ISR -> FOC -> PWM -> Motor
```

### Original characteristics

The original software was centered on direct motor control:

- direct control path from ISR into FOC and PWM
- no explicit safety layer
- no clear separation of logical modules
- no state machine
- no central fault manager
- no operator-level protocol or API
- no enforced order of operations

### Original control style

Control was performed directly in `main.c` and the existing control modules,
typically through calls such as:

- `FOC_OpenLoop_Position(...)`
- `FOC_PositionRegulation(...)`

### Original trajectory handling

Trajectory logic was local and ad hoc, for example:

```c
if (traj.traj_done) {
    /* change direction */
}
```

### Original communication model

UART was used mainly as a debug stream:

- no normalized command protocol
- no operator command layer
- no API bridge for agent or GUI

### Original limitations

| Area | Limitation |
| --- | --- |
| safety | no coordinated safety logic |
| control access | no central gate for motion permission |
| diagnostics | limited and fragmented |
| scalability | difficult to extend cleanly |
| integration | no agent/API/GUI contract |

## New version

### New architectural intent

The expanded software adds:

- a dedicated safety layer
- a communication layer
- separation of concerns
- explicit state management
- commissioning stages
- telemetry and event logging
- an agent/API/GUI integration path

### Architectural change

#### Before

```text
FOC -> Motor
```

#### After (design target)

```text
GUI
  -> Agent / API
    -> Protocol
      -> AxisControl
        -> FOC
          -> Motor
```

In the current repository, the GUI and a minimal local REST agent are present,
while the firmware still defaults to a safe-integration build that keeps real
motion disabled until the original current-feedback and full motion path are
restored.

## New layers and modules

### Communication layer

The new system adds `protocol.*`, which introduces:

- parsing of normalized commands
- structured `GET / SET / CMD` semantics

Examples:

```text
CMD ENABLE
CMD MOVE_REL 100
SET PARAM MAX_CURRENT 0.3
```

### Control layer

The new system adds `axis_control.*` as a mediator between operator commands
and the existing control path.

Its role is:

- validate whether motion is allowed
- translate commands into control requests
- prevent uncontrolled motion requests

### Safety layer

The new system adds:

- `fault.*`
- `watchdogs.*`
- `limits.*`
- `safety_config.*`

This turns safety into explicit software ownership instead of implicit behavior.

### State machine

The new system adds an explicit axis state model. In the current repository the
runtime states include:

```text
BOOT -> CONFIG / SAFE -> CALIBRATION -> ARMED -> READY -> MOTION -> STOPPING -> FAULT
```

`CONFIG` now represents a deliberate waiting state when configuration or
calibration is still incomplete, instead of silently collapsing everything into
`SAFE`.

### Commissioning

The new system adds staged startup logic:

| Stage | Meaning |
| --- | --- |
| 1 | SAFE |
| 2 | ARMING |
| 3 | CONTROLLED MOTION |

This introduces ordered activation rather than immediate direct motion.

### Telemetry

The new system adds `telemetry.*` and a normalized runtime sample stream such
as:

```text
TEL,ts,pos,pos_set,vel,vel_set,iq_ref,iq_meas,state,fault
```

### Event log

The new system adds `eventlog.*` for recent structured events such as:

- boot
- fault set / clear
- commissioning stage changes
- operator commands
- calibration success

The current firmware also exposes the recent log to the agent through
`GET EVENT_COUNT` and `GET EVENT_POP`, allowing the agent to serve
`/api/events/recent`.

### API and GUI path

The new system now supports a local REST path with:

- `/api/status`
- `/api/cmd/*`
- `/api/params`
- `/api/telemetry/latest`
- `/api/events/recent`

The current repository includes both the static GUI and a minimal Python agent
that translates those REST calls into the STM32 UART protocol.

## Functional comparison

### Motion control

#### Before

- motion control invoked directly in code
- no central permission gate

#### After

- motion requests go through `AxisControl`
- motion is allowed only when `RUN_ALLOWED == 1`

### Safety

#### Before

- no coordinated software safety model

#### After

- watchdog supervision
- fault manager
- configurable limits
- safe mode and commissioning gates

### Communication

#### Before

- debug UART stream

#### After

- command protocol
- local agent/API path
- GUI/HMI integration path

### Calibration

#### Before

- implicit or weakly defined behavior

#### After

- explicit calibration command
- explicit validity state

## RUN_ALLOWED

One of the most important new concepts is the centralized motion gate:

```c
if (!AxisControl_RunAllowed())
{
    /* motion blocked */
}
```

The intended conditions are:

- no active faults
- commissioning stage 3
- `safe_mode == 0`
- calibration valid
- motion explicitly enabled

The current firmware additionally reports build-time motion availability so the
agent/GUI can distinguish between "commissioning preconditions are met" and
"this build still has motion compiled out for safe integration".

This is a major architectural shift away from direct unrestricted control.

## Changes in `main.c`

### Before

The original flow included:

- open-loop startup
- automatic trajectory cycling
- UART debug streaming

### After

The new main loop is organized around system services:

```c
while (1)
{
    Protocol_Process();
    Telemetry_Flush();
    Watchdogs_Update();
    AxisControl_BackgroundTask();
}
```

This moves the firmware toward a system-oriented structure rather than a
single-purpose motor loop.

## Changes in ISR behavior

### Before

- ISR focus was primarily FOC and low-level control

### After

- ISR path still stays real-time focused
- telemetry sampling and control gating are integrated more deliberately
- business workflow remains outside ISR

The callback order is intentionally structured around:

1. current measurement
2. position update
3. real-time control guard
4. telemetry sample creation
5. FOC only when allowed

## Conflicts solved by the new design

### UART conflict

#### Problem

- more than one potential UART producer

#### Solution

- one protocol path owns operator communication

### Trajectory conflict

#### Problem

- automatic direction changes in local control logic

#### Solution

- motion requests controlled by `AxisControl`

### Startup behavior

#### Problem

- immediate motion on startup could surprise the operator

#### Solution

- explicit safe startup and staged commissioning

## New capabilities

The expanded system enables:

- operator GUI control
- HMI integration
- REST/API bridge through the agent
- traceability and diagnostics
- structured commissioning workflow

## System impact

### Advantages

- improved safety
- better control over transitions
- modularity
- scalability
- better operator predictability

### Cost

- more software complexity
- more code volume
- more integration work

## Most important change

The firmware stopped being only a motor control program and became a supervised
control system with safety and operator-awareness.

That is the key architectural evolution.

## Summary

### Original version

- worked
- was simple
- was direct
- was comparatively unsafe and hard to scale

### Expanded version

- is safer
- is more controlled
- is more modular
- is more extensible
- is aligned with agent and GUI integration

## Conclusion

The software expansion was not primarily about improving the FOC algorithm.

It was about:

- adding system-level layers
- introducing control and sequencing
- making safety explicit
- making the firmware fit into a complete STM32 + Agent + GUI system

This is why the modernized design is much closer to a production control system
than to a standalone motor driver.
