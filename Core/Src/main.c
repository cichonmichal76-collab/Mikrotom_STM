/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "opamp.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "math.h"
#include "motorState.h"
#include "foc.h"
#include "trajectory.h"
#include "pid.h"
#include "focMath.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

volatile MotorState state;
volatile FieldOrientedControl foc;
volatile MotorTrajectory traj;
volatile PID_Controller_t pos_pid, vel_pid, vd_pid, vq_pid;

float theta_e, zad_vref, przyrost_theta_e;
volatile uint32_t moj_cnt, coIleZmianaKierunku;

uint32_t MessageLength;
char Message[99];
volatile uint32_t uartWD, uartWD_Attemps;

volatile float mojZadanyPrad, mojaZadAmpPradu;

volatile float mojaZadPredk;

volatile float mojaZadPos, pos_A, pos_B;
volatile float czasTrajektorii;


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_OPAMP1_Init();
  MX_OPAMP2_Init();
  MX_TIM1_Init();
  MX_ADC2_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  	  uartWD = 0;
  	  uartWD_Attemps = 100;

  	  mojZadanyPrad = 0.0;
  	  mojaZadAmpPradu = 0.0;
  	  mojaZadPredk = 0.0;
  	  mojaZadPos = 0.0;
  	  pos_A = 0.01;
  	  pos_B = -0.01;
  	  czasTrajektorii = 1.0;

	MotorState_Init(&state);
	FOC_Init(&foc, &htim1);

	//Inicjalizacja pos_pid
	pid_set_kp(&pos_pid, 10.0);
	pid_set_ki(&pos_pid, 0.0);
	pid_set_kd(&pos_pid, 0.0);
	pid_set_ts(&pos_pid, (float)TIM1->ARR/170000000*(float)foc.posLoopFreqDiv*2);
	pid_set_deadband(&pos_pid, 5e-6);
	pid_set_max_out(&pos_pid, 0.75);
	pid_set_max_out_dynamic(&pos_pid, 100);
	pid_set_d_filter_fc(&pos_pid, 1000.0);
	pid_set_max_d(&pos_pid, 0);
	pid_set_max_integral(&pos_pid, 0.25);

	/*
	 * Pasmo regulatora prędkości 5-10x wolniejsze niż prądowego
	 * Kp = m *wc /kf = 2kg * 177rad/s / 47N = 7.53
	 * Ki = m *wc^2 /kf = 2kg * 177^2 /47N = 1333.0
	 */

	//Inicjalizacja vel_pid
	pid_set_kp(&vel_pid, 7.53);
	pid_set_ki(&vel_pid, 1333.0);
	pid_set_kd(&vel_pid, 0.0);
	pid_set_ts(&vel_pid, (float)TIM1->ARR/170000000*(float)foc.velLoopFreqDiv*2);
	pid_set_deadband(&vel_pid, 1e-6);
	pid_set_max_out(&vel_pid, 3.0);
	pid_set_max_out_dynamic(&vel_pid, 100);
	pid_set_d_filter_fc(&vel_pid, 1000.0);
	pid_set_max_d(&vel_pid, 0);
	pid_set_max_integral(&vel_pid, 3.0);

	/*
	 * Pasmo regulatora prądowego
	 * tau = 5.64 ms <--> 177 rad/s
	 * wc = k *R /L = 5 *5 /0.0282 = 886 rad/s @k=5-RegulatorSrednioSzybki, k>10b.szybki, ryzyko oscylacji
	 * Kp = L * wc  = 0.0282 * 886 = 25
	 * Ki = R * wc = 5 * 886 = 4430
	 */

	//Inicjalizacja vd_pid
	pid_set_kp(&vd_pid, 25.0);
	pid_set_ki(&vd_pid, 4430.0);
	pid_set_kd(&vd_pid, 0.0);
	pid_set_ts(&vd_pid, (float)TIM1->ARR/170000000*(float)foc.currentLoopFreqDiv*2);
	pid_set_deadband(&vd_pid, 1e-6);
	pid_set_max_out(&vd_pid, 7.0);
	pid_set_max_out_dynamic(&vd_pid, 100);
	pid_set_d_filter_fc(&vd_pid, 1000.0);
	pid_set_max_d(&vd_pid, 0);
	pid_set_max_integral(&vd_pid, 5.0);

	//Inicjalizacja vq_pid
	pid_set_kp(&vq_pid, 25.0);
	pid_set_ki(&vq_pid, 4430.0);
	pid_set_kd(&vq_pid, 0.0);
	pid_set_ts(&vq_pid, (float)TIM1->ARR/170000000*(float)foc.currentLoopFreqDiv*2);
	pid_set_deadband(&vq_pid, 1e-6);
	pid_set_max_out(&vq_pid, 7.0);
	pid_set_max_out_dynamic(&vq_pid, 100);
	pid_set_d_filter_fc(&vq_pid, 1000.0);
	pid_set_max_d(&vq_pid, 0);
	pid_set_max_integral(&vq_pid, 5.0);


  /*
   * Uruchomienie opampów od shunt resistors
   */
	HAL_OPAMP_Start(&hopamp1);
	HAL_OPAMP_Start(&hopamp2);

  /*
   * Włączenie tranzystorów PWM
   */
	HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_3);
	HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_3);

	/*
	 * Uruchomienie kanału synchronizacji wyzwalania ADC
	 */
	HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_4);

	/* Uruchomienie ADC */
	HAL_ADCEx_InjectedStart(&hadc2);
	HAL_ADCEx_InjectedStart_IT(&hadc1);

//	MotorState_InitVbusMeasure(&state, &hadc1);

	/*
	 * URUCHOMIENIE GŁÓWNEJ PĘTLI STERUJĄCEJ
	 */
	HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);

	/*
	 * WYPOZYCJONOWANIE
	 */

	EncoderStart(0);

	float temp_u, temp_v, temp_w;
	float temp_pwm_u, temp_pwm_v, temp_pwm_w;

	foc.max_PWM = TIM1->ARR * 0.9;

	przyrost_theta_e = 0.0;
	theta_e = 0.0;
	zad_vref = 0.0;
	coIleZmianaKierunku = 5000;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
  {
		//Sprawdzenie czy silnik jest wypozycjonowany, jezeli nie jest to wypozycjonuje
		if(!state.HomingStarted){
		    FOC_MotorHomeStart(&state, &foc);
		}

		if(traj.traj_done){
			if(traj.dest_pos_m == pos_A){
				Trajectory_InitTrapezoid(&traj, state.pos_m, pos_B, czasTrajektorii);
			}
			else {
				Trajectory_InitTrapezoid(&traj, state.pos_m, pos_A, czasTrajektorii);
			}
		}

/*
		da = (1 + sin (theta_e))/2;
		db = (1 + sin(theta_e + M_PI*2/3))/2;
		dc = (1 + sin(theta_e - M_PI*2/3))/2;


		temp_pwm_u = foc.max_PWM * da;
		temp_pwm_v = foc.max_PWM * db;
		temp_pwm_w = foc.max_PWM * dc;

		FOC_SetPWMduty(&foc, (uint16_t)temp_pwm_u, (uint16_t)temp_pwm_v, (uint16_t)temp_pwm_w);
*/
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV8;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
    	HAL_GPIO_WritePin(debugOut_GPIO_Port, debugOut_Pin, GPIO_PIN_SET);

    	state.VBUS_ADC = HAL_ADC_GetValue(hadc);
    	MotorState_VBUS_Calculate(&state);

    	HAL_GPIO_WritePin(debugOut_GPIO_Port, debugOut_Pin, GPIO_PIN_RESET);

    	/*
    	 * Tu wstawic zabezpieczenie UVLO itp.
    	 */
    }
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1){

//    	HAL_GPIO_TogglePin(debugOut_GPIO_Port, debugOut_Pin);

    	/*
    	 * POMIAR PRĄDU W SILNIKU
    	 */
    	MotorState_UpdateCurrent(&state, &hadc1, &hadc2);

    	/*
    	 * AKTUALIZACJA AKTUALNEJ POZYCJI I PREDKOSCI
    	 */
    	MotorState_Update(&state, &htim4, state.dt_s);

    	/*
    	 * AKTUALIZACJA ZADANEJ POZYCJI I PRĘDKOŚCI
    	 */
    	Trajectory_Update(&traj, state.dt_s);

        /*
         * Pętla FOC
         */
    	switch (foc.FocState) {
			case foc_PosReg:
		    	FOC_PositionRegulation(&foc, &state, &traj,&vd_pid, &vq_pid, &vel_pid, &pos_pid);
				break;
			case foc_VelReg:
				FOC_VelocityRegulation(&foc, &state, &vd_pid, &vq_pid, &vel_pid);
				break;
			case foc_CurrReg:
		    	FOC_CurrentRegulation(&foc, &state, &vd_pid, &vq_pid);
				break;
			case foc_PosOpenLoop:
				FOC_OpenLoop_Position(&foc, &state);
				break;
			default:
				foc.iq_ref = 0;
		    	FOC_CurrentRegulation(&foc, &state, &vd_pid, &vq_pid);
				break;
		}

	}
}


void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{

    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
    	/*
    	 * ROZPOCZECIE POMIARU PRADU
    	 */
    }

    /*
     * PĘTLA STERUJĄCA
     */

    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1){
    	//Pętla 10 kHz

    	/*
    	 * Pętla pozycjonowania Home
    	 */
    	foc.HomingLoopCnt++;
    	if(foc.HomingLoopCnt >= 10){
    		foc.HomingLoopCnt = 0;
        	if(!state.HomingSuccessful && state.HomingOngoing){
            	FOC_MotorHomeUpdate(&state, &foc, &traj, 0.001); //wyzwalanie co 1kHz
        	}
    	}

    	/*
    	 * Pętla pomiaru VBUS
    	 */
    	state.Vbus_freqCnt++;
    	if(state.Vbus_freqCnt >= 10){
    		state.Vbus_freqCnt = 0;
    		MotorState_VbusMeasureStart(&state, &hadc1);
    	}

    	moj_cnt++;

		if(moj_cnt%10 == 0){
			if(!(HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX)){
			  	MessageLength = sprintf(Message,"%d;%d;%d;%d;%d\r\n",
			  			(int32_t)(state.current_U*1000),
						(int32_t)(state.current_V*1000),
						(int32_t)(state.current_W*1000),
						(int32_t)state.pos_um,
						(int32_t)(state.vel_m_s*1000));
			   	HAL_UART_Transmit_IT(&huart2, Message, MessageLength);
				uartWD = 0;
			}
			else {
				uartWD++;
			}

			if(uartWD>=(uartWD_Attemps)){
				HAL_UART_DeInit(&huart2);
				HAL_UART_Init(&huart2);
				uartWD = 0;
			}

		}

		//mojZadanyPrad = mojaZadAmpPradu * sinf(10*traj.t_elapsed_s);	//zadanie sin przebiegu pradu

		if(moj_cnt%coIleZmianaKierunku == 0){
			//theta_e = -theta_e;	//zmiana kierunku
			//mojZadanyPrad = -mojZadanyPrad;	//skokowa zmiana pradu
			//foc.vel_ref = -foc.vel_ref;
			//pos_A = -pos_A;
			//traj.dest_pos_m =
			//Trajectory_InitTrapezoid(&traj, state.pos_m, pos_A, czasTrajektorii);
			//mojaZadPos = -mojaZadPos;
			if (state.pos_m >= state.endstop_Left_m-0.005) {
				//foc.vel_ref = -foc.vel_ref;
			}
			if (state.pos_m <= state.endstop_Right_m+0.005) {
				//foc.vel_ref = -foc.vel_ref;
			}
		}
		if(moj_cnt>60000){
			moj_cnt = 1;	//wyzerowanie zmiennej
		}
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
   //Wykrywanie przepełnienia licznika enkodera
   if(htim->Instance == TIM4){
       if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
           state.encoder_ext--;
       else
    	   state.encoder_ext++;
   }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
