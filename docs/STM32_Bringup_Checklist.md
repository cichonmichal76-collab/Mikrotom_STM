# STM32 Bring-up Checklist

## Purpose

This checklist is the practical sequence for moving from a safe firmware flash
to the first controlled motion. It assumes the original FOC and hardware timing
remain unchanged, while the new safety, protocol, agent, and GUI layers are
being integrated.

## Build 0 - Bench Preparation

- Confirm the mechanics can move without hitting hard stops.
- Keep the power stage supply current-limited for first tests.
- Keep an external emergency stop or supply cutoff within reach.
- Flash only a build that starts in SAFE and does not auto-start motion.
- Confirm `APP_SAFE_INTEGRATION`, interlock, and build flags match the test rig.

## Stage 1 - Communication Only

- Power the MCU logic without enabling motor motion.
- Connect the agent to the STM32 UART.
- Verify `GET STATE`.
- Verify `GET FAULT`.
- Verify `GET EVENT_COUNT`.
- Verify `GET VBUS_VALID`.
- Verify `GET VBUS`.
- Verify the GUI shows SAFE or CONFIG and does not enable motion controls.
- Stop if `VBUS_VALID` never becomes `1`.
- Stop if VBUS is implausible, unstable, or reports a fault unexpectedly.

## Stage 2 - Arming Logic Without Motion

- Set commissioning stage 2.
- Send `CMD ENABLE` and verify the state transition is logical only.
- Send `CMD DISABLE` and verify outputs remain inhibited.
- Send `CMD STOP` and verify the setpoint freezes at the current position.
- Send `CMD QSTOP` and verify the firmware disables motion permission.
- Disconnect the agent heartbeat and verify communication timeout behavior.

## Stage 3 - Controlled Motion Readiness

- Set conservative limits before motion.
- Use low current, low velocity, and low acceleration values.
- Confirm calibration or homing status is valid.
- Confirm `RUN_ALLOWED` becomes true only after all gates are satisfied.
- Confirm STOP and QSTOP remain visible and reachable in the GUI.
- Confirm no legacy UART debug stream conflicts with protocol traffic.

## First Controlled Move

- Command a very small relative move.
- Watch measured position, setpoint, velocity, current, and VBUS.
- Verify motion direction matches the expected mechanical direction.
- Verify the axis stops at the requested position without oscillation.
- Repeat with a slightly larger move only after the first move is stable.

## Fault Injection Checks

- Lower current limits temporarily and verify overcurrent fault behavior safely.
- Use conservative simulated or controlled thresholds to verify VBUS faults.
- Force a communication timeout and verify motion permission is removed.
- Try a command outside soft limits and verify it is rejected.
- Verify fault clear requires an explicit operator action.

## Do Not Proceed If

- Any boot sequence energizes the motor unexpectedly.
- STOP or QSTOP does not immediately inhibit outputs.
- VBUS is missing or physically implausible.
- Current readings are zero, saturated, swapped, or noisy beyond expectation.
- The axis can move while stage is lower than 3.
- The axis can move with an active fault.
- The GUI and firmware disagree about motion availability.
