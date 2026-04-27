/*
 * foc.c
 *
 *  Created on: Mar 6, 2026
 *      Author: lukasz.sklodowski
 */

/*
 * plik zawiera funkcje sterujace silnikiem w trybie FOC
 */

#include "foc.h"
#include "compensation.h"


void FOC_Init (FieldOrientedControl *f, TIM_HandleTypeDef *t){
	f->FocState = foc_PosOpenLoop;
	f->max_PWM = t->Instance->ARR * 0.9;
	t->Instance->CCR4 = t->Instance->ARR - 1;
	f->ialfa = 0.0;
	f->ibeta = 0.0;
	f->id = 0.0;
	f->iq = 0.0;
	f->id_ref = 0.0;
	f->iq_ref = 0.0;
	f->vd_ref = 0.0;
	f->vq_ref = 0.0;
	f->valfa = 0.0;
	f->vbeta = 0.0;

	f->pos_m_err = 0.0;
	f->vel_ms_err = 0.0;
	f->id_err = 0.0;
	f->iq_err = 0.0;

	f->current_IIR_alfa = 0.8;

	f->posLoopCnt = 0;
	f->velLoopCnt = 0;
	f->currentLoopCnt = 0;
	f->HomingLoopCnt = 0;
	f->posLoopFreqDiv = 1;//20;//100;
	f->velLoopFreqDiv = 1;//5;//10;
	f->currentLoopFreqDiv = 1;
	f->HomingLoopDiv = 10;

	f->OpenLoopPositionTheta = 0.0;
}

void SVPWM( float v_alpha,  float v_beta,  float Vdc,
		 float *d_u,  float *d_v, float *d_w){

    // --- 1. Obliczenie kąta i amplitudy wektora referencyjnego ---
    float angle = atan2f(v_beta, v_alpha);   // [-pi..pi]
    //float angle = fast_atan2f_precise(v_beta, v_alpha);   // [-pi..pi]
    if (angle < 0) angle += 2.0f * M_PI;     // [0..2pi]


    float Vref = sqrtf(v_alpha * v_alpha + v_beta * v_beta);

    // --- 2. Wyznaczenie sektora (co 60°) ---
    int sector = (int)floorf(angle / (M_PI / 3.0f)) + 1;
    if (sector > 6) sector = 6;

    // --- 3. Obliczenie czasów T1 i T2 (zgodnie z teorią SVM) ---
    // Wzory: Microchip Docs – przesunięcie osi o +30°  [3](https://onlinedocs.microchip.com/oxy/GUID-AC0E172C-9656-4397-A490-08DF807DE2E8-en-US-2/GUID-E064C18E-FA54-4DD4-9E8B-31A3EDAFD643.html)
    float angle_mod = angle - (sector - 1) * (M_PI / 3.0f);
    float T1 = (sqrtf(3.0f) * Vref / Vdc) * sinf((M_PI / 3.0f) - angle_mod);
    float T2 = (sqrtf(3.0f) * Vref / Vdc) * sinf(angle_mod);

    // --- 4. Czas zerowy ---
    float T0 = 1.0f - (T1 + T2);
    if (T0 < 0) T0 = 0;         // zabezpieczenie w overmodulation

    float Ta, Tb, Tc;

    // --- 5. Mapowanie czasów na fazy dla danego sektora ---
    // Sekwencje zgodne z klasyczną strukturą SVM  [1](https://www.electromds.com/drive/pmsm-vector-space-pwm-svpwm/)
    switch(sector)
    {
        case 1:
            Ta = T0/2.0f;
            Tb = Ta + T1;
            Tc = Tb + T2;
            break;

        case 2:
            Tb = T0/2.0f;
            Ta = Tb + T2;
            Tc = Ta + T1;
            break;

        case 3:
            Tb = T0/2.0f;
            Tc = Tb + T1;
            Ta = Tc + T2;
            break;

        case 4:
            Tc = T0/2.0f;
            Tb = Tc + T2;
            Ta = Tb + T1;
            break;

        case 5:
            Tc = T0/2.0f;
            Ta = Tc + T1;
            Tb = Ta + T2;
            break;

        default: // sector 6
            Ta = T0/2.0f;
            Tc = Ta + T2;
            Tb = Tc + T1;
            break;
    }

    // --- 6. Zwrócenie wypełnień ---
    *d_u = Ta;
    *d_v = Tb;
    *d_w = Tc;

}

void FOC_SetPWMduty (FieldOrientedControl *f, uint16_t PWM_U, uint16_t PWM_V, uint16_t PWM_W){

	PWM_U = (PWM_U < 0) ? 0 : PWM_U;
	PWM_U = (PWM_U > f->max_PWM) ? f->max_PWM : PWM_U;

	PWM_V = (PWM_V < 0) ? 0 : PWM_V;
	PWM_V = (PWM_V > f->max_PWM) ? f->max_PWM : PWM_V;

	PWM_W = (PWM_W < 0) ? 0 : PWM_W;
	PWM_W = (PWM_W > f->max_PWM) ? f->max_PWM : PWM_W;

	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_U);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_V);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, PWM_W);
}


/*
 * Sterowanie silnikiem PMSM w trybie open-loop po pozycji zadanej.
 * Pozycja zadana -> generacja theta -> Valfa, Vbeta -> SVPWM.
 */

void FOC_OpenLoop_Position(
        FieldOrientedControl *f,
		MotorState *s
){

    //---------------------------------------------------------------------
    // 1. Generacja wektora Valfa i Vbeta (open-loop)
    //---------------------------------------------------------------------

    f->valfa = f->vq_ref * cosf(f->OpenLoopPositionTheta);	//Vd = 0
    f->vbeta = f->vq_ref * sinf(f->OpenLoopPositionTheta);	//Vd = 0

    //---------------------------------------------------------------------
    // 2. SVPWM
    //---------------------------------------------------------------------

    float da, db, dc;
    SVPWM(f->valfa, f->vbeta, s->Vbus, &da, &db, &dc);

    float pwma = f->max_PWM * da;
    float pwmb = f->max_PWM * db;
    float pwmc = f->max_PWM * dc;

    //---------------------------------------------------------------------
    // 3. Ustawienie wypełnień
    //---------------------------------------------------------------------
    FOC_SetPWMduty(f, (uint16_t)pwma, (uint16_t)pwmb, (uint16_t)pwmc);
}

void FOC_CurrentRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid){

	 //Transformacja Clarke'a
	f->ialfa = s->current_U;
	f->ibeta = (s->current_U + 2 * s->current_V)/M_SQRT3;

	 //Transformacja Park'a
	float cosTheta = cosf(s->theta_mech_rad);
	float sinTheta = sinf(s->theta_mech_rad);

	f->id = f->ialfa * cosTheta + f->ibeta * sinTheta;
	f->iq = -1 * f->ialfa * sinTheta+ f->ibeta * cosTheta;

	f->id_ref = 0.0;

	/* Clamping max currnet */
    if (f->iq_ref > s->maxcurrent) {
    	f->iq_ref = s->maxcurrent;
    }
    else if (f->iq_ref < -s->maxcurrent) {
    	f->iq_ref = -s->maxcurrent;
    }

	f->id_err = f->id_ref - f->id;
	f->iq_err = f->iq_ref - f->iq;

	f->vd_ref = pi_control(vd_pid, f->id_err);
	f->vq_ref = pi_control(vq_pid, f->iq_err);

	 //Odwrotna transformacja Parka
	f->valfa = f->vd_ref * cosTheta - f->vq_ref * sinTheta;
	f->vbeta = f->vd_ref * sinTheta + f->vq_ref * cosTheta;

	// SVPWM
	float da, db, dc;
	SVPWM(f->valfa, f->vbeta, s->Vbus, &da, &db, &dc);

	float pwma, pwmb, pwmc;

	pwma = f->max_PWM * da;
	pwmb = f->max_PWM * db;
	pwmc = f->max_PWM * dc;

	FOC_SetPWMduty(f, (uint16_t)pwma, (uint16_t)pwmb, (uint16_t)pwmc);
}

void FOC_VelocityRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid,
		PID_Controller_t *vel_pid){

	//Regulator prędkości
	f->velLoopCnt++;
	if(f->velLoopCnt >= f->velLoopFreqDiv){	//petla wykonywana 10x rzadziej
		f->velLoopCnt = 0;
		f->vel_ms_err = f->vel_ref - s->vel_m_s; //Policzenie błędu prędkości zadanej trajektorii i aktualnej

	    //IIR filter do usuniecia wyzszych harmonicznych ze względu na hałas,
		//ale generalnie najlepiej jakby go nie używać.
		float setCurrentIq = pi_control(vel_pid, f->vel_ms_err);
		float baseIqRef = f->current_IIR_alfa * setCurrentIq + (1-f->current_IIR_alfa) * f->iq_ref;
	    f->iq_ref = Compensation_AdjustIqReference(baseIqRef, s);
		f->id_ref = 0.0;
	}

	//Regulacja Pradu
	FOC_CurrentRegulation(f, s, vd_pid, vq_pid);
}

void FOC_PositionRegulation(
		FieldOrientedControl *f,
		MotorState *s,
		MotorTrajectory *t,
		PID_Controller_t *vd_pid,
		PID_Controller_t *vq_pid,
		PID_Controller_t *vel_pid,
		PID_Controller_t *pos_pid){


	//Regulator Położenia
	f->posLoopCnt++;
	if(f->posLoopCnt>=f->posLoopFreqDiv){	//pętla wykonywana 100x rzadziej
		f->posLoopCnt = 0;

		//Zdecydować sie czy trajektoria czy wartość zadana
		f->pos_m_err = t->pos_set_m - s->pos_m;	//Policzenie błędu położenia zadanej trajektorii i aktualnej
		f->vel_ref = Compensation_AdjustVelocityReference(pi_control(pos_pid, f->pos_m_err), s, t);
	}

	FOC_VelocityRegulation(f, s, vd_pid, vq_pid, vel_pid);
}

void FOC_ReleaseMotor(FieldOrientedControl *f, MotorState *s){
	f->FocState = foc_PosOpenLoop;
	f->vq_ref = 0.0;
	f->vd_ref = 0.0;
	f->OpenLoopPositionTheta = 0.0;

	s->HomingOngoing = 0;
	s->HomingSuccessful = 0;
}

void FOC_MotorHomeStart(MotorState *s, FieldOrientedControl *f){
	s->HomingStarted = 1;
	s->HomingSuccessful = 0;
	s->HomingOngoing = 1;
	s->HomingStep = 0;
	ResetZeroPosition(s);


	f->FocState = foc_PosOpenLoop;
	f->OpenLoopPositionTheta = 0.0;
	f->vq_ref = 0.0;
	f->HomingDesiredTheta = 0.0;
	f->HomingTimer_s = 0.0;
}


void FOC_MotorHomeUpdate(MotorState *s, FieldOrientedControl *f, MotorTrajectory *t,  float dt_s){
	/*
	 * Żeby wypozycjonować silnik należy wyzerować flagę zakończenie
	 * Funkcję FOC_Motor należy umieścić w pętli przerwania wyzwalanego z f np. 10 kHz.
	 * dt - okres co ile jest wyzwalana funkcja MotorHome - minimalna f  1 kHz
	 */
	/*
	 * Procedura Motor Home służy do wybazowania silnika względem elektrycznego kąta "0".
	 * Polega na:
	 *
	 * 0. Ramp-up prądu od 0 do 3 A w ciągu 1 sekundy.
	 * 1. Przemieszczanie do góry/przodu do czasu, gdy openTheta == 2pi,
	 * 		wtedy należy sprawdzić prędkość, jeżeli prędkość spadła poniżej progu,
	 * 		to znaczy, że silnik się zablokował i należy iść w drugą stronę -> KROK 2
	 * 		a jeżeli prędkość NIE SPADŁA poniżej progu -> KROK 3
	 * 2. Jeżeli silnik był zablokowany to przemieszczaj się do dołu/tyłu,
	 * 		gdy OpenTheta == -2pi należy sprawdzić prędkość,
	 * 		jeżeli prękość spadła poniżej progu,
	 * 		to znaczy, że silnik się zablokował i nie da się go wypozycjonować.
	 * 3. Ustaw OpenLoopTheta = 0, Zaczekaj sekundę, Zresetuj Enkoder.
	 * 4. Włącz regulacje stałej prędkości i dojedź do lewej, a następnie do prawej pozycji
	 * 		zapisz położenie skrajnej pozycji.
	 * 5. Włącz regulację położenia.
	 */


	f->HomingThetaTick = 2 * M_PI * dt_s;	//2pi/s == 30 mm/s
	float maxVqRef = 5.0;	//Ustalanie "mocy" pozycjonowania
	float VqRefIncreaseStep = maxVqRef * dt_s *1;	//*1 sekunda

	float absVel, minimumVel_ms;
	absVel = fabsf(s->vel_m_s);
	minimumVel_ms = 0.010; 	//próg wykrycia zatrzymania

	switch (s->HomingStep) {
		case 0:
			//Ramp-up napięcia na silniku od 0 do 5V.

			if (f->vq_ref < maxVqRef) {
				f->vq_ref += VqRefIncreaseStep;
			}
			else {
				f->HomingTimer_s += dt_s;
				f->vq_ref = maxVqRef;

				if(f->HomingTimer_s >= 1.0){	//Przytrzymja obecne ustawienia na 0.1 s
					s->HomingStep = 1;	//Przejdź do następnego kroku
					f->HomingTimer_s = 0.0;	//Wyzeruj timer pomocniczny
				}
			}
			break;

		case 1:
			/*
			 * W tym kroku OpenTheta będzie przemieszczać się
			 * o jeden tick, do czasu osiągniecia 2pi
			 * wtedy zostanie sprawdzone czy silnik się w ogóle poruszył.
			 */
			if(f->OpenLoopPositionTheta >= (2*M_PI)){	//Open Theta przemieściła się o 2pi
				//Sprwadź prędkość
				if (s->vel_m_s >= minimumVel_ms) {	//Prędkość jest powyżej progu, silnik NIE zablokował się
					s->HomingStep = 3; //Przejdź do kroku 3
				}
				else {	//Silnik zablokował się
					//Należy przemieścić się w drugą stronę.
					s->HomingStep = 2;
				}
			}
			//Przemieszczenie do "góry/przodu" o jeden tick
			f->OpenLoopPositionTheta += f->HomingThetaTick;
			break;

		case 2:
			/*
			 * Ten krok zostaje wykonany, ponieważ w poprzednim kroku,
			 * silnik nie poruszył się, więc trzeba go przemieścić w drugą stronę.
			 */

			if(f->OpenLoopPositionTheta <= (-2*M_PI)){	//Open Theta przemieściła się do -2pi
				//Sprwadź prędkość
				if (s->vel_m_s <= -minimumVel_ms) {	//Prędkość jest powyżej progu, silnik NIE zablokował się
					s->HomingStep = 3; //Przejdź do kroku 3
				}
				else {	//Silnik zablokował się
					//POZYCJONOWANIE NIE POWIODŁO SIĘ
					FOC_ReleaseMotor(f, s);
					break;
				}
			}
			//Przemieszczenie do "góry/przodu" o jeden tick
			f->OpenLoopPositionTheta -= f->HomingThetaTick;
			break;

		case 3:

			f->OpenLoopPositionTheta = 0.0;	//Ustaw OpenLoopTheta = 0
			f->HomingTimer_s += dt_s; //Licz czas, który minął

			if(f->HomingTimer_s >= 1.0){	//1 sekunda na ustabilizowanie w pozycji theta == 0
				ResetZeroPosition(s);
				f->HomingTimer_s = 0.0;
				s->HomingStep = 4;
			}
			break;

		case 4:
			/*
			 * Teraz określenie Górnego/Przedniego end stopa
			 * W tym celu sterowanie przechodzi w tryb stałej prędkości
			 * Silnik porusza się do czasu gdy Prędkość spadnie poniżej progu
			 * i jest on uznawany za Górny/Przedni endstop
			 */

			f->HomingTimer_s += dt_s;

			/*
			 * Włączenie trybu stałej prędkości
			 */
			if(f->FocState != foc_VelReg){
				//Włączenie trybu stałej prędkości
				f->FocState = foc_VelReg;
				f->vel_ref = 0.05;	//Z taką prędkością silnik będzie się poruszał do GÓRY/PRZODU
			}

			/*
			 * Jeżeli silnik poruszał się co najmniej 0.25 sekundy
			 * i prędkość spadła poniżęj progu
			 * to znaczy, że się zatrzymał
			 */
			if(f->HomingTimer_s >= 0.25 && absVel <= minimumVel_ms){
				/*
				 * Silnik dojechał do skrajnej górnej/przedniej pozycji
				 */
				s->endstop_Left_m = s->pos_m;
				s->HomingStep = 5;
				f->vel_ref = -f->vel_ref;//Zadaj nową prędkość - odwrotność
				f->HomingTimer_s = 0.0;	//Wyzeruj timer
			}
			break;

		case 5:
			/*
			 * Teraz określenie Prawego End Stopa
			 */

			/*
			 * Jeżeli silnik poruszał się co najmniej 0.25 sekundy
			 * i prędkość spadła poniżęj progu
			 * to znaczy, że się zatrzymał
			 */

			f->HomingTimer_s += dt_s;

			if(f->HomingTimer_s >= 0.25 && absVel <= minimumVel_ms){
				/*
				 * Silnik dojechał do skrajnej górnej/przedniej pozycji
				 */
				s->endstop_Right_m = s->pos_m;

				//Zadaj regulacje pozycji
				f->FocState = foc_PosReg;

				//Zadaj trajektorie - idź na środek.
				Trajectory_InitTrapezoid(t, s->pos_m, ((s->endstop_Left_m+s->endstop_Right_m)/2), 1);

				//Ustaw flagi
				s->HomingSuccessful = 1;
				s->HomingOngoing = 0;

			}
			break;

		default:
			//Jeżeli wpisano złą wartość to zacznij pozycjonowanie od początku
			FOC_MotorHomeStart(s, f);
			break;

	}
}
