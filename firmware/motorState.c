/*
 * motorState.c
 */

#include "motorState.h"

void EncoderStart (uint16_t offset){

    htim4.Instance->CNT = offset;

    __HAL_TIM_URS_ENABLE(&htim4);
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
    HAL_TIM_Encoder_Start_IT(&htim4, TIM_CHANNEL_ALL);
}

void MotorState_Init(MotorState *s){

    s->dt_s = (float)TIM1->ARR/170000000.0f*4.0f;

    s->encoder_ext = 0;
    s->pos_um = 0.0f;
    s->pos_prev_m = 0.0f;
    s->pos_m = 0.0f;
    s->theta_mech_rad = 0.0f;
    s->theta_elec_cmd_rad = 0.0f;

    s->vel_raw_m_s = 0.0f;
    s->vel_m_s = 0.0f;
    s->vel_alpha = 0.9f;

    s->mass_kg = 1.0f;
    s->viscous_B = 0.0f;
    s->max_force_N = 120.0f;

    s->um_per_elec_rad = 30000.0f;
    s->encoder_um_offset = 0.0f;
    s->encoder_um_zero = 0.0f;

    s->current_alpha = 0.85f;
    s->current_U_ADC = 0;
    s->current_U_ADC_offset = 2464;
    s->current_V_ADC = 0;
    s->current_V_ADC_offset = 2455;

    s->current_U = 0.0f;
    s->current_V = 0.0f;
    s->current_W = 0.0f;
    s->current_U_raw = 0.0f;
    s->current_V_raw = 0.0f;

    s->Vbus = 12.0f;
    s->maxcurrent = 3.0f;

    s->MeasureZeroCurrentFlag = 0u;
    s->ZeroCurrent_ADC = 0.0f;
    s->maxValCurr = 0.0f;
    s->minValCurr = 0.0f;

    s->pos_set_m = 0.0f;
    s->vel_set_m_s = 0.0f;
    s->enabled = 0u;
    s->calibrated = 0u;
}

void MotorState_Update(MotorState *s, TIM_HandleTypeDef *EncoderTimer, float dt_s){
    int32_t cnt = (s->encoder_ext << 16) | (int32_t)__HAL_TIM_GET_COUNTER(EncoderTimer);

    s->pos_prev_m = s->pos_m;
    s->pos_um = (float)cnt;
    s->pos_m  = s->pos_um * 1e-6f;

    const float v_raw = (s->pos_m - s->pos_prev_m) / dt_s;
    s->vel_raw_m_s = v_raw;
    s->vel_m_s     = s->vel_alpha * v_raw + (1.0f - s->vel_alpha) * s->vel_m_s;

    s->theta_mech_rad = 2.0f * M_PI * (s->pos_um + s->encoder_um_offset - s->encoder_um_zero) / s->um_per_elec_rad;
}

/* UWAGA:
   Poniższa funkcja zostawiona jako stub-kompatybilność.
   Wklej tutaj swoją oryginalną implementację z projektu. */
void MotorState_UpdateCurrent(MotorState *s, ADC_HandleTypeDef *adc_first, ADC_HandleTypeDef *adc_second)
{
    (void)s;
    (void)adc_first;
    (void)adc_second;
}
