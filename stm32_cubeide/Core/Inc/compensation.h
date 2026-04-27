#ifndef INC_COMPENSATION_H_
#define INC_COMPENSATION_H_

#include <stdint.h>
#include "motorState.h"
#include "trajectory.h"

void Compensation_Init(void);
void Compensation_LoadDefaults(void);

int32_t Compensation_AdjustTargetUm(int32_t requested_target_um,
                                    int32_t current_pos_um);

float Compensation_AdjustVelocityReference(float base_vel_ref_m_s,
                                           const volatile MotorState *state,
                                           const volatile MotorTrajectory *traj);

float Compensation_AdjustIqReference(float base_iq_ref_a,
                                     const volatile MotorState *state);

#endif
