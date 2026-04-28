/*
 * motorState.c
 *
 *  Created on: Mar 9, 2026
 *      Author: lukasz.sklodowski
 */

/*
 * Plik zawiera funkcje i zmienne o stanie silnika:
 * - jego parametry fizyczne
 * - jego położenie
 * - jego prędkość
 */

#include "motorState.h"

void EncoderStart (uint16_t initVal){

	htim4.Instance->CNT = initVal;

	__HAL_TIM_URS_ENABLE(&htim4);          // Update tylko na overflow/underflow
	__HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);  // Włącz przerwanie UPDATE
	HAL_TIM_Encoder_Start_IT(&htim4, TIM_CHANNEL_ALL);
}

void ResetZeroPosition (MotorState *s){

	//Zerowanie wartości enkodera
	htim4.Instance->CNT = 0;
	s->encoder_ext = 0;

	//Zerowanie położenia i prędkości
	s->pos_um = 0.0;
	s->pos_m = 0.0;
	s->pos_prev_m = 0.0;
	s->theta_mech_rad = 0.0;
	s->vel_raw_m_s = 0.0;
	s->vel_m_s = 0.0;
}

void MotorState_Init(MotorState *s){

	s->VBUS_ADC = 0;
    s->Vbus = 12.0;
    s->Vbus_temp = 0.0;
	s->Vbus_freqCnt = 0;

	s->dt_s = (float)TIM1->ARR/170000000*2;	//ARR/clock freq

	s->encoder_ext = 0;
    s->pos_um = 0.0;
    s->pos_prev_m = 0.0;
    s->pos_m = 0.0;
    s->theta_mech_rad = 0.0;
    s->theta_elec_cmd_rad = 0.0;

    s->vel_raw_m_s = 0.0;
    s->vel_m_s = 0.0;
    s->vel_alpha = 0.1;

    s->mass_kg = 1.0;
    s->viscous_B = 0.0;
    s->max_force_N = 120.0;

    s->um_per_elec_rad = 30000;
    s->encoder_um_offset = 0.0;
    s->encoder_um_zero = 0.0;

    s->current_alpha = 0.1;
    s->current_U_ADC = 0;
    s->current_U_ADC_offset = 2464;
    s->current_V_ADC = 0;
    s->current_V_ADC_offset = 2455;

    s->maxcurrent = 3.0;

    s->endstop_Left_m = 0.0;
    s->endstop_Right_m = 0.0;

    s->HomingStarted = 0;
    s->HomingSuccessful = 0;
    s->HomingOngoing = 0;
    s->HomingStep = 0;
}

void MotorState_Update(MotorState *s, TIM_HandleTypeDef *EncoderTimer, float dt_s){

    s->pos_prev_m = s->pos_m; /* MINUS */

    /* count -> µm -> m (uwzględnij um_per_count) */
	int32_t cnt = (s->encoder_ext << 16)|(int32_t)__HAL_TIM_GET_COUNTER(EncoderTimer);

    s->pos_um = (float)cnt;
    s->pos_m  = s->pos_um * 1e-6;

    /* prędkość surowa i filtrowana */
    const float v_raw = (s->pos_m - s->pos_prev_m) / dt_s;
    s->vel_raw_m_s = v_raw;
    s->vel_m_s     = s->vel_alpha * v_raw + (1.0 - s->vel_alpha) * s->vel_m_s;

    /* kąt mechaniczny (rad elektryczny) z regresji: (um + offset - zero)/um_per_elec_rad */
    s->theta_mech_rad = 2* M_PI * (s->pos_um + s->encoder_um_offset - s->encoder_um_zero) / s->um_per_elec_rad;
}

void MotorState_UpdateCurrent(MotorState *s, ADC_HandleTypeDef *adc_first, ADC_HandleTypeDef *adc_second){

//	HAL_GPIO_WritePin(debug_out_GPIO_Port, debug_out_Pin, GPIO_PIN_SET);
	s->current_U_ADC = HAL_ADCEx_InjectedGetValue(adc_first, ADC_INJECTED_RANK_1); //get result from adc
	s->current_V_ADC = HAL_ADCEx_InjectedGetValue(adc_second, ADC_INJECTED_RANK_1);
//	HAL_GPIO_WritePin(debug_out_GPIO_Port, debug_out_Pin, GPIO_PIN_RESET);

	s->current_U_raw = ((float)s->current_U_ADC - (float)s->current_U_ADC_offset) * 0.048;
	s->current_V_raw = ((float)s->current_V_ADC - (float)s->current_V_ADC_offset) * 0.048;

	s->current_U = s->current_U * (1-s->current_alpha) + s->current_U_raw * s->current_alpha;
	s->current_V = s->current_V * (1-s->current_alpha) + s->current_V_raw * s->current_alpha;

	s->current_W = -(s->current_U + s->current_V);

	if(s->current_U > s->maxValCurr) s->maxValCurr = s->current_U;
	if(s->current_V > s->maxValCurr) s->maxValCurr = s->current_V;
	if(s->current_W > s->maxValCurr) s->maxValCurr = s->current_W;

	if(s->current_U < s->minValCurr) s->minValCurr = s->current_U;
	if(s->current_V < s->minValCurr) s->minValCurr = s->current_V;
	if(s->current_W < s->minValCurr) s->minValCurr = s->current_W;
}

void MotorState_VBUS_Calculate(MotorState *s){
	/*
	 * Konwersja ADC -> Volts
	 *
	 * ADC_raw / ADC_res * Vcc * ResDividerRatio = VBUS_V
	 */

	//Zainicjalizowanie zmiennych
	float
		MCU_Vcc = 3.3,
		ResDividerRatio = 10.4,	// 18k/(18k+169k) =
		ADC_Res = 4096.0,
		VBUS_V = 0.0;

	VBUS_V = (float)s->VBUS_ADC /ADC_Res * MCU_Vcc * ResDividerRatio;

	s->Vbus_temp = VBUS_V;
}

void MotorState_VbusMeasureStart (MotorState *s, ADC_HandleTypeDef *adc){
	s->VBUS_ADC = 0;
	HAL_ADC_Start_IT(adc);
}


