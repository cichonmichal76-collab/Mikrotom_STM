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
#include "app_build_config.h"

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
#include "safety_monitor.h"
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
volatile uint32_t uartWD;

volatile float mojZadanyPrad, mojaZadAmpPradu;
volatile float mojaZadPredk;
volatile float mojaZadPos, pos_A, pos_B;
volatile float czasTrajektorii;
#define HOMING_SOFT_LIMIT_GUARD_UM 500
#ifndef APP_EXTERNAL_INTERLOCK_ACTIVE_STATE
#define APP_EXTERNAL_INTERLOCK_ACTIVE_STATE GPIO_PIN_RESET
#endif
static uint8_t g_homing_sequence_active = 0u;
static uint8_t g_homing_request_pending = 0u;

static void MotionOutputs_Disable(void)
{
    foc.id_ref = 0.0f;
    foc.iq_ref = 0.0f;
    foc.vd_ref = 0.0f;
    foc.vq_ref = 0.0f;
    foc.vel_ref = 0.0f;
    FOC_SetPWMduty((FieldOrientedControl*)&foc, 0u, 0u, 0u);
}

#if !APP_SAFE_INTEGRATION
static int32_t MotionControl_MToUm(float position_m)
{
    return (int32_t)(position_m * 1000000.0f);
}

static void MotionControl_CompleteHoming(void)
{
    int32_t endstop_left_um = MotionControl_MToUm(state.endstop_Left_m);
    int32_t endstop_right_um = MotionControl_MToUm(state.endstop_Right_m);

    if (!Limits_SetMeasuredTravelUm(endstop_left_um, endstop_right_um, HOMING_SOFT_LIMIT_GUARD_UM))
    {
        g_homing_sequence_active = 0u;
        Fault_Set(FAULT_CONFIG_INVALID);
        MotionOutputs_Disable();
        return;
    }

    g_homing_request_pending = 0u;
    Calibration_SetZeroPosUm(0);
    Calibration_SetValid(1u);
    state.calibrated = 1u;
    g_homing_sequence_active = 0u;
    EventLog_Push(EVT_CALIB_OK, endstop_right_um - endstop_left_um);
    AxisControl_NotifyCalibrationComplete(1u);
    (void)ConfigStore_Save();
}

static void MotionControl_UpdateHomingState(void)
{
    if (!g_homing_sequence_active)
        return;

    if (state.HomingSuccessful && traj.traj_done)
    {
        MotionControl_CompleteHoming();
        return;
    }

    if (state.HomingStarted && !state.HomingOngoing && !state.HomingSuccessful)
    {
        g_homing_request_pending = 0u;
        g_homing_sequence_active = 0u;
        MotionOutputs_Disable();
    }
}
#endif

static void MotionControl_UpdateExternalInterlock(void)
{
#if !APP_SAFE_INTEGRATION && defined(PushButton_Pin) && defined(PushButton_GPIO_Port)
    const SafetyConfig_t *safety = SafetyConfig_Get();

    if (!safety->external_interlock_installed || safety->ignore_external_interlock)
        return;

    if (HAL_GPIO_ReadPin(PushButton_GPIO_Port, PushButton_Pin) == APP_EXTERNAL_INTERLOCK_ACTIVE_STATE)
    {
        g_homing_request_pending = 0u;
        g_homing_sequence_active = 0u;
        state.enabled = 0u;
        Fault_Set(FAULT_ESTOP);
        MotionOutputs_Disable();
    }
#endif
}

static void MotionControl_UpdateOutputInhibit(void)
{
#if !APP_SAFE_INTEGRATION
    if (g_homing_sequence_active)
        return;

    if (Fault_IsActive() || !state.enabled ||
        (AxisState_Get() == AXIS_STOPPING) ||
        (AxisState_Get() == AXIS_SAFE) ||
        (AxisState_Get() == AXIS_ESTOP))
    {
        MotionOutputs_Disable();
    }
#endif
}

uint8_t MotionControl_RequestHoming(void)
{
#if APP_SAFE_INTEGRATION
    return 0u;
#else
    if (Fault_IsActive()) return 0u;
    if (g_homing_sequence_active) return 0u;
    if (state.HomingOngoing) return 0u;
    if (state.HomingSuccessful) return 1u;

    g_homing_request_pending = 1u;
    AxisState_Set(AXIS_CALIBRATION);
    EventLog_Push(EVT_CALIB_START, 0);
    return 1u;
#endif
}

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
    SafetyMonitor_Init();
    ConfigStore_Init();
    Calibration_Init();
    AxisControl_Init();
    Protocol_Init();

    ConfigStore_Load();
    state.maxcurrent = Limits_Get()->max_current_peak_A;
    EventLog_Push(EVT_BOOT, 0);
    AxisControl_RefreshState();

    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp2);

#if !APP_SAFE_INTEGRATION
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start_IT(&htim1, TIM_CHANNEL_3);
    EncoderStart(0);
#endif

    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_4);
    HAL_ADCEx_InjectedStart(&hadc2);
    HAL_ADCEx_InjectedStart_IT(&hadc1);
    HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);

    foc.max_PWM = TIM1->ARR * 0.9f;
    przyrost_theta_e = 0.0f;
    theta_e = 0.0f;
    zad_vref = 0.0f;
    coIleZmianaKierunku = 5000;
  /* USER CODE END 2 */

  while (1)
  {
#if !APP_SAFE_INTEGRATION
        MotionControl_UpdateExternalInterlock();
        if (g_homing_request_pending && !Fault_IsActive() && !state.HomingStarted)
        {
            g_homing_request_pending = 0u;
            g_homing_sequence_active = 1u;
            FOC_MotorHomeStart((MotorState*)&state, (FieldOrientedControl*)&foc);
        }
        MotionControl_UpdateHomingState();
#endif
        Protocol_Process();
        MotionControl_UpdateOutputInhibit();
        Telemetry_Flush();
        Watchdogs_Update();
        MotionControl_UpdateExternalInterlock();
        MotionControl_UpdateOutputInhibit();
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

        if (SafetyMonitor_UpdateRt((const volatile MotorState*)&state,
                                   (const volatile MotorTrajectory*)&traj,
                                   g_homing_sequence_active))
        {
            state.enabled = 0u;
            g_homing_request_pending = 0u;
            g_homing_sequence_active = 0u;
            MotionOutputs_Disable();
            return;
        }

#if !APP_SAFE_INTEGRATION
        if (!g_homing_sequence_active)
            AxisControl_UpdateRt();
#else
        AxisControl_UpdateRt();
#endif

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
        if (!g_homing_sequence_active && !AxisControl_RunAllowed())
        {
            state.enabled = 0u;
            MotionOutputs_Disable();
            return;
        }
        switch (foc.FocState) {
            case foc_PosReg:
                FOC_PositionRegulation((FieldOrientedControl*)&foc, (MotorState*)&state, (MotorTrajectory*)&traj, (PID_Controller_t*)&vd_pid, (PID_Controller_t*)&vq_pid, (PID_Controller_t*)&vel_pid, (PID_Controller_t*)&pos_pid);
                break;
            case foc_VelReg:
                FOC_VelocityRegulation((FieldOrientedControl*)&foc, (MotorState*)&state, (PID_Controller_t*)&vd_pid, (PID_Controller_t*)&vq_pid, (PID_Controller_t*)&vel_pid);
                break;
            case foc_CurrReg:
                FOC_CurrentRegulation((FieldOrientedControl*)&foc, (MotorState*)&state, (PID_Controller_t*)&vd_pid, (PID_Controller_t*)&vq_pid);
                break;
            case foc_PosOpenLoop:
                FOC_OpenLoop_Position((FieldOrientedControl*)&foc, (MotorState*)&state);
                break;
            default:
                foc.iq_ref = 0.0f;
                FOC_CurrentRegulation((FieldOrientedControl*)&foc, (MotorState*)&state, (PID_Controller_t*)&vd_pid, (PID_Controller_t*)&vq_pid);
                break;
        }
#endif
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        state.VBUS_ADC = HAL_ADC_GetValue(hadc);
        MotorState_VBUS_Calculate((MotorState*)&state);
    }
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
    }

    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1){
        moj_cnt++;

#if APP_VBUS_MEASUREMENT_ENABLED
        state.Vbus_freqCnt++;
        if (state.Vbus_freqCnt >= 10u) {
            state.Vbus_freqCnt = 0u;
            MotorState_VbusMeasureStart((MotorState*)&state, &hadc1);
        }
#endif

#if !APP_SAFE_INTEGRATION
        foc.HomingLoopCnt++;
        if (foc.HomingLoopCnt >= 10u) {
            foc.HomingLoopCnt = 0u;
            if (!state.HomingSuccessful && state.HomingOngoing) {
                FOC_MotorHomeUpdate((MotorState*)&state, (FieldOrientedControl*)&foc, (MotorTrajectory*)&traj, 0.001f);
            }
        }
#endif

#if !APP_SAFE_INTEGRATION && APP_LEGACY_UART_STREAMING
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
#endif

#if !APP_SAFE_INTEGRATION
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
