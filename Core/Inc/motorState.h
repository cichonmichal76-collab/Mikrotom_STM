/*
 * motorState.h
 *
 *  Created on: Mar 9, 2026
 *      Author: lukasz.sklodowski
 */

#ifndef INC_MOTORSTATE_H_
#define INC_MOTORSTATE_H_

#include <stdint.h>
#include "tim.h"
#include "adc.h"
#include "math.h"

/* ========= MotorState ========= */
typedef struct {

	uint16_t VBUS_ADC;	//Surowy pomiar napiecia zasilania silnikow
    float
		Vbus,	//Napiecie zasilajace
		Vbus_temp;	//Zmienna tymczasowa do weryfikacji pomiaru VBUS
    uint32_t Vbus_freqCnt;

	float dt_s; /* Okres pętli sprawdzajacej położenie, NIE OKRES PRACY PWM */

    /* Pozycja (z enkodera) */
	volatile int32_t encoder_ext;
    float pos_um;            /* [µm]               */
    float pos_m;             /* [m]                */
    float pos_prev_m;        /* [m] poprzedni stan */

    /* Kąty */
    float theta_mech_rad;      /* [rad] z regresji µm->rad(elec) */
    float theta_elec_cmd_rad;  /* [rad] transport pola (NCO)     */

    /* Prędkości */
    float vel_raw_m_s;       /* [m/s] surowa (różniczkowana) */
    float vel_m_s;           /* [m/s] filtrowana IIR (EMA)   */

    /* Parametry filtrów prędkości (jeśli będziesz rozwijać) */
    float vel_alpha;         /* 0..1 — współczynnik EMA      */

    /* Parametry fizyczne silnika */
    float mass_kg;            /* [kg]        */
    float viscous_B;          /* [N·s/m]     */
    float max_force_N;        /* [N]         */

    /* Regresja µm -> rad(elektryczny)
       theta_elec = (pos_um + encoder_um_offset - encoder_um_zero) / um_per_elec_rad */
    float um_per_elec_rad;    /* [µm/rad]    */
    float encoder_um_offset;  /* [µm]        */
    float encoder_um_zero;    /* [µm]        */

    /* Prądy w uzwojeniach */

    float current_alpha;	/* Wsp. [0-1] do filtracji IIR prądu */
    uint16_t
    	current_U_ADC,
    	current_V_ADC,
    	current_U_ADC_offset,
    	current_V_ADC_offset;

    float
    	current_U,
    	current_V,
		current_W,
    	current_U_raw,
    	current_V_raw;

    float
		maxcurrent;



    uint16_t MeasureZeroCurrentFlag;
    float ZeroCurrent_ADC;

    float maxValCurr, minValCurr;

    /*
     * ZMIENNE DO POZYCJONOWANIA
     */
	float
		endstop_Left_m,	// Położenie skrajne
		endstop_Right_m;

	uint32_t
		HomingStarted,	//Flaga Rozpoczecia pozycjonowania
		HomingSuccessful,	//Flaga zakończenia pozycjonowania
		HomingOngoing,	//Flaga trwania pozycjonowania
		HomingStep;	//Numer kroku pozycjonowania

} MotorState;

void MotorState_Init(MotorState *s);
void MotorState_Update(MotorState *s, TIM_HandleTypeDef *EncoderTimer, float dt_s);
void EncoderStart (uint16_t initVal);
void ResetZeroPosition (MotorState *s);
void MotorState_VbusMeasureStart (MotorState *s, ADC_HandleTypeDef *adc);
void MotorState_VBUS_Calculate(MotorState *s);

#endif /* INC_MOTORSTATE_H_ */
