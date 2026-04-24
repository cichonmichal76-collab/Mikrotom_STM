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
#include "safety_monitor.h"
#include <math.h>

extern volatile MotorState state;
extern volatile MotorTrajectory traj;

static AxisState_t axis_control_idle_state(void);

static float axis_control_compute_move_time_s(float distance_m)
{
    const Limits_t *limits = Limits_Get();
    float abs_distance_m = fabsf(distance_m);
    float min_time_from_acc_s = 0.0f;
    float min_time_from_vel_s = 0.0f;
    float move_time_s;

    if (abs_distance_m <= 0.0f)
        return 0.0f;

    if (limits->max_acceleration_m_s2 > 0.0f)
        min_time_from_acc_s = 2.0f * sqrtf(abs_distance_m / limits->max_acceleration_m_s2);

    if (limits->max_velocity_m_s > 0.0f)
        min_time_from_vel_s = (2.0f * abs_distance_m) / limits->max_velocity_m_s;

    move_time_s = (min_time_from_acc_s > min_time_from_vel_s) ? min_time_from_acc_s : min_time_from_vel_s;
    if (move_time_s < (2.0f * state.dt_s))
        move_time_s = 2.0f * state.dt_s;

    return move_time_s;
}

static uint8_t axis_control_start_move_um(int32_t target_um)
{
    float target_m = ((float)target_um) * 1e-6f;
    float distance_m = target_m - state.pos_m;
    float move_time_s;

    if (fabsf(distance_m) <= 0.5e-6f)
    {
        state.pos_set_m = target_m;
        state.vel_set_m_s = 0.0f;
        traj.traj_done = 1;
        AxisState_Set(axis_control_idle_state());
        return 1u;
    }

    move_time_s = axis_control_compute_move_time_s(distance_m);
    if (move_time_s <= 0.0f)
        return 0u;

    Trajectory_InitTrapezoid((MotorTrajectory*)&traj, state.pos_m, target_m, move_time_s);
    state.pos_set_m = traj.pos_set_m;
    state.vel_set_m_s = Limits_ClampVelocity(traj.vel_set_m_s);
    AxisState_Set(AXIS_MOTION);
    return 1u;
}

static uint8_t axis_preconditions_ok(void)
{
    const SafetyConfig_t *s = SafetyConfig_Get();
    if (Fault_IsActive()) return 0u;
    if (Commissioning_SafeMode()) return 0u;
    if (!SafetyMonitor_PowerReady((const volatile MotorState*)&state)) return 0u;
#if APP_MOTION_IMPLEMENTED
    if (!state.HomingSuccessful) return 0u;
#endif
    if (!Calibration_IsValid() && !s->allow_motion_without_calibration) return 0u;
    return 1u;
}

static AxisState_t axis_control_idle_state(void)
{
    const SafetyConfig_t *s = SafetyConfig_Get();
    uint8_t calibration_required = (!Calibration_IsValid() && !s->allow_motion_without_calibration) ? 1u : 0u;
#if APP_MOTION_IMPLEMENTED
    uint8_t homing_required = state.HomingSuccessful ? 0u : 1u;
#else
    uint8_t homing_required = 0u;
#endif

    if (Fault_IsActive()) return AXIS_FAULT;
    if ((calibration_required || homing_required) && !state.enabled) return AXIS_CONFIG;
    if (Commissioning_SafeMode()) return AXIS_SAFE;
    if (!state.enabled) return AXIS_SAFE;
    if (Commissioning_ArmingOnly()) return AXIS_ARMED;
    if (calibration_required || homing_required) return AXIS_CONFIG;
    return AXIS_READY;
}

void AxisControl_Init(void){}

void AxisControl_RefreshState(void)
{
    AxisState_t current = AxisState_Get();

    if ((current == AXIS_MOTION) || (current == AXIS_STOPPING) || (current == AXIS_CALIBRATION))
        return;

    AxisState_Set(axis_control_idle_state());
}

uint8_t AxisControl_RunAllowed(void)
{
    if (!axis_preconditions_ok()) return 0u;
    if (!AxisState_CanMove()) return 0u;
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
    traj.dest_pos_m = state.pos_m;
    traj.pos_set_m = state.pos_m;
    traj.vel_set_m_s = 0.0f;
    state.enabled = 0u;
    state.pos_set_m = state.pos_m;
    state.vel_set_m_s = 0.0f;
    AxisState_Set(AXIS_STOPPING);
    EventLog_Push(EVT_STOP, 0);
    return 1u;
}

uint8_t AxisControl_QStop(void)
{
    traj.traj_done = 1;
    traj.dest_pos_m = state.pos_m;
    traj.pos_set_m = state.pos_m;
    traj.vel_set_m_s = 0.0f;
    state.enabled = 0u;
    state.pos_set_m = state.pos_m;
    state.vel_set_m_s = 0.0f;
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
    int64_t target_wide = (int64_t)current_pos + (int64_t)delta_um;
    int32_t target;
    if (!AxisControl_RunAllowed()) return 0u;
    if (!Limits_IsMoveRelAllowed(current_pos, delta_um)) return 0u;
    if ((target_wide < (int64_t)Limits_Get()->soft_min_pos_um) ||
        (target_wide > (int64_t)Limits_Get()->soft_max_pos_um)) return 0u;
    target = (int32_t)target_wide;
    target = Limits_ClampPositionUm(target);
    return axis_control_start_move_um(target);
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
    return axis_control_start_move_um(target_um);
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
        traj.dest_pos_m = state.pos_m;
        traj.pos_set_m = state.pos_m;
        traj.vel_set_m_s = 0.0f;
        state.pos_set_m = state.pos_m;
        state.vel_set_m_s = 0.0f;
        AxisState_Set(AXIS_FAULT);
        return;
    }

    if (!AxisControl_RunAllowed())
    {
        traj.traj_done = 1;
        traj.dest_pos_m = state.pos_m;
        traj.pos_set_m = state.pos_m;
        traj.vel_set_m_s = 0.0f;
        state.pos_set_m = state.pos_m;
        state.vel_set_m_s = 0.0f;
        return;
    }

    if (AxisState_Get() == AXIS_MOTION)
    {
        traj.vel_set_m_s = Limits_ClampVelocity(traj.vel_set_m_s);
        state.pos_set_m = traj.pos_set_m;
        state.vel_set_m_s = traj.vel_set_m_s;
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
