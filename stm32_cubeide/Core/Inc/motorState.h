/*
 * motorState.h
 */

#ifndef INC_MOTORSTATE_H_
#define INC_MOTORSTATE_H_

#include <stdint.h>
#include "tim.h"
#include "adc.h"
#include "math.h"

typedef struct {

    uint16_t VBUS_ADC;
    float Vbus_temp;
    uint32_t Vbus_freqCnt;
    uint8_t Vbus_valid;

    float dt_s;

    volatile int32_t encoder_ext;
    float pos_um;
    float pos_m;
    float pos_prev_m;

    float theta_mech_rad;
    float theta_elec_cmd_rad;

    float vel_raw_m_s;
    float vel_m_s;
    float vel_alpha;

    float mass_kg;
    float viscous_B;
    float max_force_N;

    float um_per_elec_rad;
    float encoder_um_offset;
    float encoder_um_zero;

    float current_alpha;
    uint16_t current_U_ADC, current_V_ADC, current_U_ADC_offset, current_V_ADC_offset;

    float current_U, current_V, current_W, current_U_raw, current_V_raw;

    float maxcurrent;
    float Vbus;

    uint16_t MeasureZeroCurrentFlag;
    float ZeroCurrent_ADC;

    float maxValCurr, minValCurr;

    float endstop_Left_m;
    float endstop_Right_m;

    /* --- pola systemowe pod safety / GUI / agent --- */
    float pos_set_m;
    float vel_set_m_s;
    uint8_t enabled;
    uint8_t calibrated;

    uint32_t HomingStarted;
    uint32_t HomingSuccessful;
    uint32_t HomingOngoing;
    uint32_t HomingStep;

} MotorState;

void MotorState_Init(MotorState *s);
void MotorState_Update(MotorState *s, TIM_HandleTypeDef *EncoderTimer, float dt_s);
void EncoderStart (uint16_t offset);
void ResetZeroPosition (MotorState *s);
void MotorState_VbusMeasureStart(MotorState *s, ADC_HandleTypeDef *adc);
void MotorState_VBUS_Calculate(MotorState *s);
void MotorState_UpdateCurrent(MotorState *s, ADC_HandleTypeDef *adc_first, ADC_HandleTypeDef *adc_second);

#endif
