#include "safety_monitor.h"
#include "fault.h"
#include "limits.h"
#include "axis_state.h"
#include <math.h>

#define SAFETY_OVERCURRENT_SAMPLES 3u
#define SAFETY_OVERSPEED_SAMPLES 20u
#define SAFETY_TRACKING_SAMPLES 100u
#define SAFETY_VBUS_FAULT_SAMPLES 100u
#define SAFETY_OVERSPEED_FACTOR 1.5f
#define SAFETY_TRACKING_ERROR_M 0.002f
#define SAFETY_SOFT_LIMIT_GUARD_UM 1000

#ifndef SAFETY_VBUS_MIN_V
#define SAFETY_VBUS_MIN_V 8.0f
#endif

#ifndef SAFETY_VBUS_MAX_V
#define SAFETY_VBUS_MAX_V 30.0f
#endif

static uint32_t g_overcurrent_samples;
static uint32_t g_overspeed_samples;
static uint32_t g_tracking_samples;
static uint32_t g_undervoltage_samples;
static uint32_t g_overvoltage_samples;

static float safety_abs3_max(float a, float b, float c)
{
    float aa = fabsf(a);
    float bb = fabsf(b);
    float cc = fabsf(c);
    float max = (aa > bb) ? aa : bb;
    return (max > cc) ? max : cc;
}

static void safety_debounce_fault(uint8_t condition,
                                  uint32_t *counter,
                                  uint32_t limit,
                                  FaultCode_t fault)
{
    if (condition)
    {
        if (*counter < limit)
            (*counter)++;
        if (*counter >= limit)
            Fault_Set(fault);
    }
    else
    {
        *counter = 0u;
    }
}

void SafetyMonitor_Init(void)
{
    g_overcurrent_samples = 0u;
    g_overspeed_samples = 0u;
    g_tracking_samples = 0u;
    g_undervoltage_samples = 0u;
    g_overvoltage_samples = 0u;
}

uint8_t SafetyMonitor_PowerReady(const volatile MotorState *s)
{
    if (s == 0) return 0u;
    if (!s->Vbus_valid) return 0u;
    if (s->Vbus < SAFETY_VBUS_MIN_V) return 0u;
    if (s->Vbus > SAFETY_VBUS_MAX_V) return 0u;
    return 1u;
}

uint8_t SafetyMonitor_UpdateRt(const volatile MotorState *s,
                               const volatile MotorTrajectory *t,
                               uint8_t homing_active)
{
    const Limits_t *limits = Limits_Get();
    float max_phase_current_A;
    uint8_t overcurrent;
    uint8_t vbus_known;
    uint8_t undervoltage;
    uint8_t overvoltage;
    uint8_t inhibit_outputs;

    if ((s == 0) || (t == 0))
        return Fault_IsActive();

    max_phase_current_A = safety_abs3_max(s->current_U, s->current_V, s->current_W);
    overcurrent = (max_phase_current_A > limits->max_current_peak_A) ? 1u : 0u;
    vbus_known = s->Vbus_valid ? 1u : 0u;
    undervoltage = (vbus_known && (s->Vbus < SAFETY_VBUS_MIN_V)) ? 1u : 0u;
    overvoltage = (vbus_known && (s->Vbus > SAFETY_VBUS_MAX_V)) ? 1u : 0u;
    /*
     * VBUS faults inhibit outputs immediately. Overcurrent is debounced below,
     * so a single noisy current sample does not silently cancel a commanded move
     * without latching FAULT_OVERCURRENT.
     */
    inhibit_outputs = (!vbus_known || undervoltage || overvoltage) ? 1u : 0u;

    safety_debounce_fault(overcurrent, &g_overcurrent_samples,
                          SAFETY_OVERCURRENT_SAMPLES, FAULT_OVERCURRENT);
    safety_debounce_fault(undervoltage, &g_undervoltage_samples,
                          SAFETY_VBUS_FAULT_SAMPLES, FAULT_UNDERVOLTAGE);
    safety_debounce_fault(overvoltage, &g_overvoltage_samples,
                          SAFETY_VBUS_FAULT_SAMPLES, FAULT_OVERVOLTAGE);

    if (!homing_active)
    {
        AxisState_t axis_state = AxisState_Get();
        int32_t pos_um = (int32_t)s->pos_um;
        uint8_t outside_soft_limits;
        uint8_t overspeed;
        uint8_t tracking_error;

        outside_soft_limits =
            ((pos_um < (limits->soft_min_pos_um - SAFETY_SOFT_LIMIT_GUARD_UM)) ||
             (pos_um > (limits->soft_max_pos_um + SAFETY_SOFT_LIMIT_GUARD_UM))) ? 1u : 0u;
        if (s->HomingSuccessful && outside_soft_limits)
            Fault_Set(FAULT_TRACKING);

        /*
         * Speed supervision is meaningful only while firmware is actively
         * commanding motion. During SAFE/ARMED startup the first encoder
         * sample may legitimately synchronize from an arbitrary counter
         * value and must not latch a false OVERSPEED fault.
         */
        overspeed =
            ((axis_state == AXIS_MOTION) &&
             (fabsf(s->vel_m_s) > (limits->max_velocity_m_s * SAFETY_OVERSPEED_FACTOR))) ? 1u : 0u;
        safety_debounce_fault(overspeed, &g_overspeed_samples,
                              SAFETY_OVERSPEED_SAMPLES, FAULT_OVERSPEED);

        tracking_error =
            ((axis_state == AXIS_MOTION) &&
             (fabsf(t->pos_set_m - s->pos_m) > SAFETY_TRACKING_ERROR_M)) ? 1u : 0u;
        safety_debounce_fault(tracking_error, &g_tracking_samples,
                              SAFETY_TRACKING_SAMPLES, FAULT_TRACKING);
    }
    else
    {
        g_overspeed_samples = 0u;
        g_tracking_samples = 0u;
    }

    return (inhibit_outputs || Fault_IsActive()) ? 1u : 0u;
}
