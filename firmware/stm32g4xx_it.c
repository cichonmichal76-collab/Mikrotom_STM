/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32g4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "protocol.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

void NMI_Handler(void){ while (1){} }
void HardFault_Handler(void){ while (1){} }
void MemManage_Handler(void){ while (1){} }
void BusFault_Handler(void){ while (1){} }
void UsageFault_Handler(void){ while (1){} }
void SVC_Handler(void){}
void DebugMon_Handler(void){}
void PendSV_Handler(void){}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void ADC1_2_IRQHandler(void)
{
  HAL_ADC_IRQHandler(&hadc1);
  HAL_ADC_IRQHandler(&hadc2);
}

void TIM1_BRK_TIM15_IRQHandler(void){ HAL_TIM_IRQHandler(&htim1); }
void TIM1_UP_TIM16_IRQHandler(void){ HAL_TIM_IRQHandler(&htim1); }
void TIM1_TRG_COM_TIM17_IRQHandler(void){ HAL_TIM_IRQHandler(&htim1); }
void TIM1_CC_IRQHandler(void){ HAL_TIM_IRQHandler(&htim1); }
void TIM2_IRQHandler(void){ HAL_TIM_IRQHandler(&htim2); }
void TIM4_IRQHandler(void){ HAL_TIM_IRQHandler(&htim4); }
void USART2_IRQHandler(void){ HAL_UART_IRQHandler(&huart2); }

/* USER CODE BEGIN 1 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        Protocol_RxCpltCallback(Protocol_GetLastRxByte());
    }
}
/* USER CODE END 1 */
