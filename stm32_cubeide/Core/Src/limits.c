#include "limits.h"

#define LIMITS_ABS_MIN_POS_UM (-50000)
#define LIMITS_ABS_MAX_POS_UM (50000)
#define LIMITS_MIN_TRAVEL_UM 100
#define LIMITS_MAX_CURRENT_NOMINAL_A 1.0f
#define LIMITS_MAX_CURRENT_PEAK_A 3.0f
#define LIMITS_MAX_VELOCITY_M_S 0.1f
#define LIMITS_MAX_ACCELERATION_M_S2 1.0f
#define LIMITS_MAX_PEAK_TIME_MS 10000u

static Limits_t g_limits;
void Limits_Init(void){ Limits_LoadDefaults(); }
void Limits_LoadDefaults(void)
{
    g_limits.soft_min_pos_um = -10000;
    g_limits.soft_max_pos_um = 10000;
    g_limits.max_current_nominal_A = 0.2f;
    g_limits.max_current_peak_A = 0.3f;
    g_limits.max_velocity_m_s = 0.005f;
    g_limits.max_acceleration_m_s2 = 0.020f;
    g_limits.peak_time_ms = 200u;
}
const Limits_t* Limits_Get(void){ return &g_limits; }
void Limits_Apply(const Limits_t *limits){ if (Limits_IsValid(limits)) g_limits = *limits; }
void Limits_SetSoftMinPos(int32_t v_um){ (void)Limits_SetSoftRangeUm(v_um, g_limits.soft_max_pos_um); }
void Limits_SetSoftMaxPos(int32_t v_um){ (void)Limits_SetSoftRangeUm(g_limits.soft_min_pos_um, v_um); }
void Limits_SetMaxCurrentNominal(float v_A){ Limits_t next = g_limits; next.max_current_nominal_A = v_A; if (Limits_IsValid(&next)) g_limits = next; }
void Limits_SetMaxCurrentPeak(float v_A){ Limits_t next = g_limits; next.max_current_peak_A = v_A; if (Limits_IsValid(&next)) g_limits = next; }
void Limits_SetMaxVelocity(float v_m_s){ Limits_t next = g_limits; next.max_velocity_m_s = v_m_s; if (Limits_IsValid(&next)) g_limits = next; }
void Limits_SetMaxAcceleration(float v_m_s2){ Limits_t next = g_limits; next.max_acceleration_m_s2 = v_m_s2; if (Limits_IsValid(&next)) g_limits = next; }
uint8_t Limits_IsValid(const Limits_t *limits)
{
    int32_t travel_um;

    if (limits == 0) return 0u;
    if (limits->soft_min_pos_um >= limits->soft_max_pos_um) return 0u;
    if (limits->soft_min_pos_um < LIMITS_ABS_MIN_POS_UM) return 0u;
    if (limits->soft_max_pos_um > LIMITS_ABS_MAX_POS_UM) return 0u;

    travel_um = limits->soft_max_pos_um - limits->soft_min_pos_um;
    if (travel_um < LIMITS_MIN_TRAVEL_UM) return 0u;

    if (!(limits->max_current_nominal_A >= 0.0f)) return 0u;
    if (!(limits->max_current_nominal_A <= LIMITS_MAX_CURRENT_NOMINAL_A)) return 0u;
    if (!(limits->max_current_peak_A >= limits->max_current_nominal_A)) return 0u;
    if (!(limits->max_current_peak_A <= LIMITS_MAX_CURRENT_PEAK_A)) return 0u;
    if (!(limits->max_velocity_m_s > 0.0f)) return 0u;
    if (!(limits->max_velocity_m_s <= LIMITS_MAX_VELOCITY_M_S)) return 0u;
    if (!(limits->max_acceleration_m_s2 > 0.0f)) return 0u;
    if (!(limits->max_acceleration_m_s2 <= LIMITS_MAX_ACCELERATION_M_S2)) return 0u;
    if (limits->peak_time_ms == 0u) return 0u;
    if (limits->peak_time_ms > LIMITS_MAX_PEAK_TIME_MS) return 0u;

    return 1u;
}

uint8_t Limits_SetSoftRangeUm(int32_t min_um, int32_t max_um)
{
    Limits_t next = g_limits;

    next.soft_min_pos_um = min_um;
    next.soft_max_pos_um = max_um;
    if (!Limits_IsValid(&next)) return 0u;

    g_limits = next;
    return 1u;
}

uint8_t Limits_SetMeasuredTravelUm(int32_t endstop_a_um, int32_t endstop_b_um, int32_t guard_um)
{
    int32_t min_um = (endstop_a_um < endstop_b_um) ? endstop_a_um : endstop_b_um;
    int32_t max_um = (endstop_a_um < endstop_b_um) ? endstop_b_um : endstop_a_um;

    if (guard_um < 0) return 0u;
    if ((max_um - min_um) <= (2 * guard_um)) return 0u;

    return Limits_SetSoftRangeUm(min_um + guard_um, max_um - guard_um);
}
int32_t Limits_ClampPositionUm(int32_t pos_um){ if(pos_um<g_limits.soft_min_pos_um) return g_limits.soft_min_pos_um; if(pos_um>g_limits.soft_max_pos_um) return g_limits.soft_max_pos_um; return pos_um; }
float Limits_ClampCurrentA(float current_A){ if(current_A>g_limits.max_current_peak_A) return g_limits.max_current_peak_A; if(current_A<-g_limits.max_current_peak_A) return -g_limits.max_current_peak_A; return current_A; }
float Limits_ClampVelocity(float vel_m_s){ if(vel_m_s>g_limits.max_velocity_m_s) return g_limits.max_velocity_m_s; if(vel_m_s<-g_limits.max_velocity_m_s) return -g_limits.max_velocity_m_s; return vel_m_s; }
uint8_t Limits_IsPositionAllowed(int32_t pos_um){ return (pos_um >= g_limits.soft_min_pos_um && pos_um <= g_limits.soft_max_pos_um) ? 1u : 0u; }
uint8_t Limits_IsMoveRelAllowed(int32_t current_pos_um, int32_t delta_um)
{
    int64_t target_um = (int64_t)current_pos_um + (int64_t)delta_um;

    if ((target_um < (int64_t)g_limits.soft_min_pos_um) ||
        (target_um > (int64_t)g_limits.soft_max_pos_um))
        return 0u;

    return 1u;
}
uint8_t Limits_IsMoveAbsAllowed(int32_t target_um){ return Limits_IsPositionAllowed(target_um); }
