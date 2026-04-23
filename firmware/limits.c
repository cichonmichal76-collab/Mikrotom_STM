#include "limits.h"
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
void Limits_Apply(const Limits_t *limits){ if (limits != 0) g_limits = *limits; }
void Limits_SetSoftMinPos(int32_t v_um){ g_limits.soft_min_pos_um = v_um; }
void Limits_SetSoftMaxPos(int32_t v_um){ g_limits.soft_max_pos_um = v_um; }
void Limits_SetMaxCurrentNominal(float v_A){ g_limits.max_current_nominal_A = v_A; }
void Limits_SetMaxCurrentPeak(float v_A){ g_limits.max_current_peak_A = v_A; }
void Limits_SetMaxVelocity(float v_m_s){ g_limits.max_velocity_m_s = v_m_s; }
void Limits_SetMaxAcceleration(float v_m_s2){ g_limits.max_acceleration_m_s2 = v_m_s2; }
int32_t Limits_ClampPositionUm(int32_t pos_um){ if(pos_um<g_limits.soft_min_pos_um) return g_limits.soft_min_pos_um; if(pos_um>g_limits.soft_max_pos_um) return g_limits.soft_max_pos_um; return pos_um; }
float Limits_ClampCurrentA(float current_A){ if(current_A>g_limits.max_current_peak_A) return g_limits.max_current_peak_A; if(current_A<-g_limits.max_current_peak_A) return -g_limits.max_current_peak_A; return current_A; }
float Limits_ClampVelocity(float vel_m_s){ if(vel_m_s>g_limits.max_velocity_m_s) return g_limits.max_velocity_m_s; if(vel_m_s<-g_limits.max_velocity_m_s) return -g_limits.max_velocity_m_s; return vel_m_s; }
uint8_t Limits_IsPositionAllowed(int32_t pos_um){ return (pos_um >= g_limits.soft_min_pos_um && pos_um <= g_limits.soft_max_pos_um) ? 1u : 0u; }
uint8_t Limits_IsMoveRelAllowed(int32_t current_pos_um, int32_t delta_um){ return Limits_IsPositionAllowed(current_pos_um + delta_um); }
uint8_t Limits_IsMoveAbsAllowed(int32_t target_um){ return Limits_IsPositionAllowed(target_um); }
