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

void ResetZeroPosition (MotorState *s)
{
    htim4.Instance->CNT = 0;
    s->encoder_ext = 0;

    s->pos_um = 0.0f;
    s->pos_m = 0.0f;
    s->pos_prev_m = 0.0f;
    s->theta_mech_rad = 0.0f;
    s->vel_raw_m_s = 0.0f;
    s->vel_m_s = 0.0f;
}

void MotorState_Init(MotorState *s){

    s->VBUS_ADC = 0u;
    s->Vbus = 0.0f;
    s->Vbus_temp = 0.0f;
    s->Vbus_freqCnt = 0u;
    s->Vbus_valid = 0u;

    s->dt_s = (float)TIM1->ARR/170000000.0f*2.0f;

    s->encoder_ext = 0;
    s->pos_um = 0.0f;
    s->pos_prev_m = 0.0f;
    s->pos_m = 0.0f;
    s->theta_mech_rad = 0.0f;
    s->theta_elec_cmd_rad = 0.0f;

    s->vel_raw_m_s = 0.0f;
    s->vel_m_s = 0.0f;
    s->vel_alpha = 0.1f;

    s->mass_kg = 1.0f;
    s->viscous_B = 0.0f;
    s->max_force_N = 120.0f;

    s->um_per_elec_rad = 30000.0f;
    s->encoder_um_offset = 0.0f;
    s->encoder_um_zero = 0.0f;

    s->current_alpha = 0.1f;
    s->current_U_ADC = 0;
    s->current_U_ADC_offset = 2464;
    s->current_V_ADC = 0;
    s->current_V_ADC_offset = 2455;

    s->current_U = 0.0f;
    s->current_V = 0.0f;
    s->current_W = 0.0f;
    s->current_U_raw = 0.0f;
    s->current_V_raw = 0.0f;

    s->maxcurrent = 3.0f;

    s->MeasureZeroCurrentFlag = 0u;
    s->ZeroCurrent_ADC = 0.0f;
    s->maxValCurr = 0.0f;
    s->minValCurr = 0.0f;

    s->endstop_Left_m = 0.0f;
    s->endstop_Right_m = 0.0f;

    s->pos_set_m = 0.0f;
    s->vel_set_m_s = 0.0f;
    s->enabled = 0u;
    s->calibrated = 0u;

    s->HomingStarted = 0u;
    s->HomingSuccessful = 0u;
    s->HomingOngoing = 0u;
    s->HomingStep = 0u;
}

void MotorState_Update(MotorState *s, TIM_HandleTypeDef *EncoderTimer, float dt_s){
    int32_t ext_before;
    int32_t ext_after;
    uint32_t counter;
    int32_t cnt;

    do
    {
        ext_before = s->encoder_ext;
        counter = __HAL_TIM_GET_COUNTER(EncoderTimer);
        ext_after = s->encoder_ext;
    } while (ext_before != ext_after);

    cnt = (ext_after << 16) | (int32_t)counter;

    s->pos_prev_m = s->pos_m;
    s->pos_um = (float)cnt;
    s->pos_m  = s->pos_um * 1e-6f;

    const float v_raw = (s->pos_m - s->pos_prev_m) / dt_s;
    s->vel_raw_m_s = v_raw;
    s->vel_m_s     = s->vel_alpha * v_raw + (1.0f - s->vel_alpha) * s->vel_m_s;

    s->theta_mech_rad = 2.0f * M_PI * (s->pos_um + s->encoder_um_offset - s->encoder_um_zero) / s->um_per_elec_rad;
}

/* Restored current-feedback path from the original controller. */
void MotorState_UpdateCurrent(MotorState *s, ADC_HandleTypeDef *adc_first, ADC_HandleTypeDef *adc_second)
{
    s->current_U_ADC = HAL_ADCEx_InjectedGetValue(adc_first, ADC_INJECTED_RANK_1);
    s->current_V_ADC = HAL_ADCEx_InjectedGetValue(adc_second, ADC_INJECTED_RANK_1);

    s->current_U_raw = ((float)s->current_U_ADC - (float)s->current_U_ADC_offset) * 0.048f;
    s->current_V_raw = ((float)s->current_V_ADC - (float)s->current_V_ADC_offset) * 0.048f;

    s->current_U = s->current_U * (1.0f - s->current_alpha) + s->current_U_raw * s->current_alpha;
    s->current_V = s->current_V * (1.0f - s->current_alpha) + s->current_V_raw * s->current_alpha;
    s->current_W = -(s->current_U + s->current_V);

    if (s->current_U > s->maxValCurr) s->maxValCurr = s->current_U;
    if (s->current_V > s->maxValCurr) s->maxValCurr = s->current_V;
    if (s->current_W > s->maxValCurr) s->maxValCurr = s->current_W;

    if (s->current_U < s->minValCurr) s->minValCurr = s->current_U;
    if (s->current_V < s->minValCurr) s->minValCurr = s->current_V;
    if (s->current_W < s->minValCurr) s->minValCurr = s->current_W;
}

void MotorState_VBUS_Calculate(MotorState *s)
{
    const float mcu_vcc = 3.3f;
    const float res_divider_ratio = 10.4f;
    const float adc_res = 4096.0f;
    float vbus_v;

    vbus_v = ((float)s->VBUS_ADC / adc_res) * mcu_vcc * res_divider_ratio;
    s->Vbus_temp = vbus_v;
    s->Vbus = vbus_v;
    s->Vbus_valid = 1u;
}

void MotorState_VbusMeasureStart(MotorState *s, ADC_HandleTypeDef *adc)
{
    s->VBUS_ADC = 0u;
    HAL_ADC_Start_IT(adc);
}
