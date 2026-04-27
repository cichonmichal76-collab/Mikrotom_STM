#include "compensation.h"

void Compensation_Init(void)
{
    Compensation_LoadDefaults();
}

void Compensation_LoadDefaults(void)
{
}

int32_t Compensation_AdjustTargetUm(int32_t requested_target_um,
                                    int32_t current_pos_um)
{
    (void)current_pos_um;
    return requested_target_um;
}

float Compensation_AdjustVelocityReference(float base_vel_ref_m_s,
                                           const volatile MotorState *state,
                                           const volatile MotorTrajectory *traj)
{
    (void)state;
    (void)traj;
    return base_vel_ref_m_s;
}

float Compensation_AdjustIqReference(float base_iq_ref_a,
                                     const volatile MotorState *state)
{
    (void)state;
    return base_iq_ref_a;
}
