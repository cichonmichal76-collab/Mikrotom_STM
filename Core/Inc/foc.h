/*
 * foc.h
 *
 *  Created on: Mar 6, 2026
 *      Author: lukasz.sklodowski
 */

#ifndef INC_FOC_H_
#define INC_FOC_H_

#include "tim.h"
#include <stdint.h>
#include "motorState.h"
#include "trajectory.h"
#include "pid.h"
#include "math.h"
#include "focMath.h"



typedef enum {
	foc_PosReg,
	foc_VelReg,
	foc_CurrReg,
	foc_PosOpenLoop
}FOC_State;

typedef struct{
	FOC_State FocState;

	uint16_t max_PWM;

	float
		PWM_period;

	float
		vel_ref,	//wartosc referencyjna prędkości - output regualatora położenia i input reg prędk.

		ialfa,
		ibeta,
		id,
		iq,
		id_ref,
		iq_ref,
		vd_ref,
		vq_ref,
		valfa,
		vbeta,

		pos_m_err,
		vel_ms_err,
		id_err,
		iq_err,

		current_IIR_alfa,

		HomingTimer_s,
		HomingThetaTick,
		HomingCheckpoint_um,
		HomingDesiredTheta,

		OpenLoopPositionTheta;

	uint32_t
		posLoopCnt,
		velLoopCnt,
		currentLoopCnt,
		HomingLoopCnt,
		posLoopFreqDiv,
		velLoopFreqDiv,
		currentLoopFreqDiv,
		HomingLoopDiv;
}FieldOrientedControl;

void FOC_Init (FieldOrientedControl *f, TIM_HandleTypeDef *t);
void FOC_SetPWMduty (FieldOrientedControl *f, uint16_t PWM_U, uint16_t PWM_V, uint16_t PWM_W);
void SVPWM(float v_alpha, float v_beta, float Vdc,
           float *d_u, float *d_v, float *d_w);
void FOC_MotorHomeStart(MotorState *s, FieldOrientedControl *f);
void FOC_MotorHomeUpdate(MotorState *s, FieldOrientedControl *f, MotorTrajectory *t,  float dt_s);

void FOC_OpenLoop_Position(
        FieldOrientedControl *f,
		MotorState *s
);

void FOC_CurrentRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid);

void FOC_VelocityRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid,
		PID_Controller_t *vel_pid);

void FOC_PositionRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		MotorTrajectory *t,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid,
		PID_Controller_t *vel_pid,
		PID_Controller_t *pos_pid);

#endif /* INC_FOC_H_ */
