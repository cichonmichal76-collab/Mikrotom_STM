#include "axis_control.h"
#include "axis_state.h"
#include "fault.h"
#include "commissioning.h"
#include "limits.h"
#include "calibration.h"
#include "safety_config.h"
#include "eventlog.h"
#include "motorState.h"
#include "trajectory.h"
#include "app_build_config.h"

extern volatile MotorState state;
extern volatile MotorTrajectory traj;

static uint8_t axis_preconditions_ok(void)
{
    const SafetyConfig_t *s = SafetyConfig_Get();
    if (Fault_IsActive()) return 0u;
    if (Commissioning_SafeMode()) return 0u;
    if (!Calibration_IsValid() && !s->allow_motion_without_calibration) return 0u;
    return 1u;
}

static AxisState_t axis_control_idle_state(void)
{
    const SafetyConfig_t *s = SafetyConfig_Get();
    uint8_t calibration_required = (!Calibration_IsValid() && !s->allow_motion_without_calibration) ? 1u : 0u;

    if (Fault_IsActive()) return AXIS_FAULT;
    if (calibration_required && !state.enabled) return AXIS_CONFIG;
    if (Commissioning_SafeMode()) return AXIS_SAFE;
    if (!state.enabled) return AXIS_SAFE;
    if (Commissioning_ArmingOnly()) return AXIS_ARMED;
    if (calibration_required) return AXIS_CONFIG;
    return AXIS_READY;
}

void AxisControl_Init(void){}

uint8_t AxisControl_RunAllowed(void)
{
    if (!axis_preconditions_ok()) return 0u;
    if (!state.enabled) return 0u;
    if (!Commissioning_ControlledMotion()) return 0u;
    return 1u;
}

uint8_t AxisControl_Enable(void)
{
    if (Fault_IsActive()) return 0u;
    state.enabled = 1u;
    AxisState_Set(axis_control_idle_state());
    EventLog_Push(EVT_ENABLE, 0);
    return 1u;
}

uint8_t AxisControl_Disable(void)
{
    state.enabled = 0u;
    AxisState_Set(AXIS_SAFE);
    EventLog_Push(EVT_DISABLE, 0);
    return 1u;
}

uint8_t AxisControl_Stop(void)
{
    traj.traj_done = 1;
    AxisState_Set(AXIS_STOPPING);
    EventLog_Push(EVT_STOP, 0);
    return 1u;
}

uint8_t AxisControl_QStop(void)
{
    traj.traj_done = 1;
    state.enabled = 0u;
    AxisState_Set(AXIS_SAFE);
    EventLog_Push(EVT_QSTOP, 0);
    return 1u;
}

uint8_t AxisControl_MoveRelUm(int32_t delta_um)
{
#if !APP_MOTION_IMPLEMENTED
    return 0u;
#else
    int32_t current_pos = (int32_t)state.pos_um;
    int32_t target = current_pos + delta_um;
    if (!AxisControl_RunAllowed()) return 0u;
    if (!Limits_IsMoveRelAllowed(current_pos, delta_um)) return 0u;
    target = Limits_ClampPositionUm(target);

    state.pos_set_m = ((float)target) * 1e-6f;
    state.vel_set_m_s = 0.0f;

    traj.start_pos_m = state.pos_m;
    traj.dest_pos_m = ((float)target) * 1e-6f;
    traj.pos_set_m = traj.dest_pos_m;
    traj.vel_set_m_s = 0.0f;
    traj.traj_done = 0;

    AxisState_Set(AXIS_MOTION);
    return 1u;
#endif
}

uint8_t AxisControl_MoveAbsUm(int32_t target_um)
{
#if !APP_MOTION_IMPLEMENTED
    return 0u;
#else
    if (!AxisControl_RunAllowed()) return 0u;
    if (!Limits_IsMoveAbsAllowed(target_um)) return 0u;
    target_um = Limits_ClampPositionUm(target_um);

    state.pos_set_m = ((float)target_um) * 1e-6f;
    state.vel_set_m_s = 0.0f;

    traj.start_pos_m = state.pos_m;
    traj.dest_pos_m = ((float)target_um) * 1e-6f;
    traj.pos_set_m = traj.dest_pos_m;
    traj.vel_set_m_s = 0.0f;
    traj.traj_done = 0;

    AxisState_Set(AXIS_MOTION);
    return 1u;
#endif
}

void AxisControl_NotifyConfigChanged(void)
{
    AxisState_t current = AxisState_Get();

    if (current == AXIS_FAULT || current == AXIS_MOTION || current == AXIS_STOPPING || current == AXIS_CALIBRATION)
        return;

    AxisState_Set(AXIS_CONFIG);
}

void AxisControl_NotifyCalibrationComplete(uint8_t success)
{
    if (!success)
    {
        AxisState_Set(AXIS_CONFIG);
        return;
    }

    AxisState_Set(axis_control_idle_state());
}

void AxisControl_UpdateRt(void)
{
    if (Fault_IsActive())
    {
        state.enabled = 0u;
        traj.traj_done = 1;
        AxisState_Set(AXIS_FAULT);
        return;
    }

    if (!AxisControl_RunAllowed())
    {
        traj.traj_done = 1;
        return;
    }

    if (state.maxcurrent > Limits_Get()->max_current_peak_A)
        state.maxcurrent = Limits_Get()->max_current_peak_A;
}

void AxisControl_BackgroundTask(void)
{
    if (AxisState_Get() == AXIS_STOPPING)
        AxisState_Set(axis_control_idle_state());
    if ((AxisState_Get() == AXIS_MOTION) && traj.traj_done)
        AxisState_Set(axis_control_idle_state());
    if (AxisState_Get() == AXIS_CONFIG)
    {
        AxisState_t next_state = axis_control_idle_state();
        if (next_state != AXIS_CONFIG) AxisState_Set(next_state);
    }
}
