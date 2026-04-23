#ifndef LIMITS_H
#define LIMITS_H
#include <stdint.h>
typedef struct {
    int32_t soft_min_pos_um;
    int32_t soft_max_pos_um;
    float max_current_nominal_A;
    float max_current_peak_A;
    float max_velocity_m_s;
    float max_acceleration_m_s2;
    uint32_t peak_time_ms;
} Limits_t;
void Limits_Init(void);
void Limits_LoadDefaults(void);
const Limits_t* Limits_Get(void);
void Limits_Apply(const Limits_t *limits);
void Limits_SetSoftMinPos(int32_t v_um);
void Limits_SetSoftMaxPos(int32_t v_um);
void Limits_SetMaxCurrentNominal(float v_A);
void Limits_SetMaxCurrentPeak(float v_A);
void Limits_SetMaxVelocity(float v_m_s);
void Limits_SetMaxAcceleration(float v_m_s2);
int32_t Limits_ClampPositionUm(int32_t pos_um);
float Limits_ClampCurrentA(float current_A);
float Limits_ClampVelocity(float vel_m_s);
uint8_t Limits_IsPositionAllowed(int32_t pos_um);
uint8_t Limits_IsMoveRelAllowed(int32_t current_pos_um, int32_t delta_um);
uint8_t Limits_IsMoveAbsAllowed(int32_t target_um);
#endif
