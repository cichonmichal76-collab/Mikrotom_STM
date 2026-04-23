/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

#include "axis_state.h"
#include "fault.h"
#include "commissioning.h"
#include "eventlog.h"
#include "telemetry.h"
#include "watchdogs.h"
#include "limits.h"
#include "safety_config.h"
#include "config_store.h"
#include "calibration.h"
#include "axis_control.h"
#include "fw_version.h"
#include "protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_SAFE_INTEGRATION 1
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
volatile uint32_t uartWD;

volatile float mojZadanyPrad, mojaZadAmpPradu;
volatile float mojaZadPredk;
volatile float mojaZadPos, pos_A, pos_B;
volatile float czasTrajektorii;

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

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
      mojZadanyPrad = 0.0f;
      mojaZadAmpPradu = 0.0f;
      mojaZadPredk = 0.0f;
      mojaZadPos = 0.0f;
      pos_A = 0.01f;
      pos_B = -0.01f;
      czasTrajektorii = 1.0f;

    MotorState_Init((MotorState*)&state);
    FOC_Init((FieldOrientedControl*)&foc, &htim1);

    pid_set_kp((PID_Controller_t*)&pos_pid, 25.0f);
    pid_set_ki((PID_Controller_t*)&pos_pid, 0.0f);
    pid_set_kd((PID_Controller_t*)&pos_pid, 0.0f);
    pid_set_ts((PID_Controller_t*)&pos_pid, (float)TIM1->ARR/170000000.0f*(float)foc.posLoopFreqDiv*2.0f);
    pid_set_deadband((PID_Controller_t*)&pos_pid, 5e-6f);
    pid_set_max_out((PID_Controller_t*)&pos_pid, 0.25f);
    pid_set_max_out_dynamic((PID_Controller_t*)&pos_pid, 100);
    pid_set_d_filter_fc((PID_Controller_t*)&pos_pid, 1000.0f);
    pid_set_max_d((PID_Controller_t*)&pos_pid, 0);
    pid_set_max_integral((PID_Controller_t*)&pos_pid, 0.25f);

    pid_set_kp((PID_Controller_t*)&vel_pid, 7.53f);
    pid_set_ki((PID_Controller_t*)&vel_pid, 1333.0f);
    pid_set_kd((PID_Controller_t*)&vel_pid, 0.0f);
    pid_set_ts((PID_Controller_t*)&vel_pid, (float)TIM1->ARR/170000000.0f*(float)foc.velLoopFreqDiv*2.0f);
    pid_set_deadband((PID_Controller_t*)&vel_pid, 1e-3f);
    pid_set_max_out((PID_Controller_t*)&vel_pid, 3.0f);
    pid_set_max_out_dynamic((PID_Controller_t*)&vel_pid, 100);
    pid_set_d_filter_fc((PID_Controller_t*)&vel_pid, 1000.0f);
    pid_set_max_d((PID_Controller_t*)&vel_pid, 0);
    pid_set_max_integral((PID_Controller_t*)&vel_pid, 3.0f);

    pid_set_kp((PID_Controller_t*)&vd_pid, 25.0f);
    pid_set_ki((PID_Controller_t*)&vd_pid, 4430.0f);
    pid_set_kd((PID_Controller_t*)&vd_pid, 0.0f);
    pid_set_ts((PID_Controller_t*)&vd_pid, (float)TIM1->ARR/170000000.0f*(float)foc.currentLoopFreqDiv*2.0f);
    pid_set_deadband((PID_Controller_t*)&vd_pid, 1e-3f);
    pid_set_max_out((PID_Controller_t*)&vd_pid, 7.0f);
    pid_set_max_out_dynamic((PID_Controller_t*)&vd_pid, 100);
    pid_set_d_filter_fc((PID_Controller_t*)&vd_pid, 1000.0f);
    pid_set_max_d((PID_Controller_t*)&vd_pid, 0);
    pid_set_max_integral((PID_Controller_t*)&vd_pid, 5.0f);

    pid_set_kp((PID_Controller_t*)&vq_pid, 25.0f);
    pid_set_ki((PID_Controller_t*)&vq_pid, 4430.0f);
    pid_set_kd((PID_Controller_t*)&vq_pid, 0.0f);
    pid_set_ts((PID_Controller_t*)&vq_pid, (float)TIM1->ARR/170000000.0f*(float)foc.currentLoopFreqDiv*2.0f);
    pid_set_deadband((PID_Controller_t*)&vq_pid, 1e-3f);
    pid_set_max_out((PID_Controller_t*)&vq_pid, 7.0f);
    pid_set_max_out_dynamic((PID_Controller_t*)&vq_pid, 100);
    pid_set_d_filter_fc((PID_Controller_t*)&vq_pid, 1000.0f);
    pid_set_max_d((PID_Controller_t*)&vq_pid, 0);
    pid_set_max_integral((PID_Controller_t*)&vq_pid, 5.0f);

    AxisState_Init();
    Fault_Init();
    Commissioning_Init();
    EventLog_Init();
    Telemetry_Init();
    Watchdogs_Init();
    Limits_Init();
    SafetyConfig_Init();
    ConfigStore_Init();
    Calibration_Init();
    AxisControl_Init();
    Protocol_Init();

    ConfigStore_Load();
    AxisState_Set(AXIS_SAFE);
    EventLog_Push(EVT_BOOT, 0);

    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp2);

#if !APP_SAFE_INTEGRATION
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_3);

    FOC_OpenLoop_Position((FieldOrientedControl*)&foc, 0.0f, 7.0f);
    HAL_Delay(1000);
    EncoderStart(0);
    Trajectory_InitTrapezoid((MotorTrajectory*)&traj, 0.0f, 0.0f, 0.1f);
    FOC_OpenLoop_Position((FieldOrientedControl*)&foc, 0.0f, 0.0f);
#endif

    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_4);
    HAL_ADCEx_InjectedStart(&hadc2);
    HAL_ADCEx_InjectedStart_IT(&hadc1);
    HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);

    foc.max_PWM = TIM1->ARR * 0.95f;
    przyrost_theta_e = 0.0f;
    theta_e = 0.0f;
    zad_vref = 0.0f;
    coIleZmianaKierunku = 5000;
  /* USER CODE END 2 */

  while (1)
  {
#if !APP_SAFE_INTEGRATION
        if(traj.traj_done){
            if(traj.dest_pos_m == pos_A){
                Trajectory_InitTrapezoid((MotorTrajectory*)&traj, state.pos_m, pos_B, czasTrajektorii);
            }
            else {
                Trajectory_InitTrapezoid((MotorTrajectory*)&traj, state.pos_m, pos_A, czasTrajektorii);
            }
        }
#endif
        Protocol_Process();
        Telemetry_Flush();
        Watchdogs_Update();
        AxisControl_BackgroundTask();
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV8;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

/* USER CODE BEGIN 4 */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1){
        MotorState_UpdateCurrent((MotorState*)&state, &hadc1, &hadc2);
        MotorState_Update((MotorState*)&state, &htim4, state.dt_s);

#if !APP_SAFE_INTEGRATION
        Trajectory_Update((MotorTrajectory*)&traj, state.dt_s);
#endif

        AxisControl_UpdateRt();

        TelemetrySample_t t;
        t.ts_ms = 0u;
        t.pos_um = (int32_t)state.pos_um;
        t.pos_set_um = (int32_t)(state.pos_set_m * 1000000.0f);
        t.vel_um_s = (int32_t)(state.vel_m_s * 1000000.0f);
        t.vel_set_um_s = (int32_t)(state.vel_set_m_s * 1000000.0f);
        t.iq_ref_mA = (int16_t)(foc.iq_ref * 1000.0f);
        t.iq_meas_mA = (int16_t)(foc.iq * 1000.0f);
        t.state = (uint8_t)AxisState_Get();
        t.fault = (uint8_t)Fault_GetLast();
        Telemetry_Sample(&t);

#if !APP_SAFE_INTEGRATION
        if (!AxisControl_RunAllowed())
        {
            state.enabled = 0u;
            return;
        }
        FOC_PositionRegulation(mojaZadPos, (FieldOrientedControl*)&foc, (MotorState*)&state, (MotorTrajectory*)&traj, (PID_Controller_t*)&vd_pid, (PID_Controller_t*)&vq_pid, (PID_Controller_t*)&vel_pid, (PID_Controller_t*)&pos_pid);
#endif
    }
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
    }

    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1){
        moj_cnt++;

#if !APP_SAFE_INTEGRATION
        if(moj_cnt%10 == 0){
            if(!(HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX)){
                HAL_GPIO_WritePin(debugOut_GPIO_Port, debugOut_Pin, GPIO_PIN_SET);
                MessageLength = sprintf(Message,"%d;%d;%d;%d;%d\r\n",
                            (int32_t)(state.current_U*1000),
                            (int32_t)(state.current_V*1000),
                            (int32_t)(state.current_W*1000),
                            (int32_t)state.pos_um,
                            (int32_t)(state.vel_m_s*1000));
                HAL_UART_Transmit_IT(&huart2, (uint8_t*)Message, MessageLength);
                HAL_GPIO_WritePin(debugOut_GPIO_Port, debugOut_Pin, GPIO_PIN_RESET);
                uartWD = 0;
            } else {
                uartWD++;
            }

            if(uartWD>=(coIleZmianaKierunku-1)){
                HAL_UART_DeInit(&huart2);
                HAL_UART_Init(&huart2);
                uartWD = 0;
            }
        }

        if(moj_cnt%coIleZmianaKierunku == 0){
        }
        if(moj_cnt>60000){
            moj_cnt = 0;
        }
#endif
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
   if(htim->Instance == TIM4){
       if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
           state.encoder_ext--;
       else
           state.encoder_ext++;
   }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
