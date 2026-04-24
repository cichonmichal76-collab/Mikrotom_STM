#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include <stdint.h>
#include "motorState.h"
#include "trajectory.h"

void SafetyMonitor_Init(void);
uint8_t SafetyMonitor_PowerReady(const volatile MotorState *s);
uint8_t SafetyMonitor_UpdateRt(const volatile MotorState *s,
                               const volatile MotorTrajectory *t,
                               uint8_t homing_active);

#endif
