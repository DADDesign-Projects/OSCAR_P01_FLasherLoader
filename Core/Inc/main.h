/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_DMA2D_Init(void);
void MX_RNG_Init(void);
void MX_SAI1_Init(void);
void MX_FMC_Init(void);
void MX_QUADSPI_Init(void);
void MX_SPI1_Init(void);
void MX_USART1_UART_Init(void);
void MX_TIM6_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ByPass_Pin GPIO_PIN_1
#define ByPass_GPIO_Port GPIOA
#define Encoder3_B_Pin GPIO_PIN_4
#define Encoder3_B_GPIO_Port GPIOH
#define Encoder3_A_Pin GPIO_PIN_5
#define Encoder3_A_GPIO_Port GPIOH
#define Encoder3_SW_Pin GPIO_PIN_3
#define Encoder3_SW_GPIO_Port GPIOA
#define TFT_Reset_Pin GPIO_PIN_5
#define TFT_Reset_GPIO_Port GPIOC
#define Encoder2_B_Pin GPIO_PIN_0
#define Encoder2_B_GPIO_Port GPIOB
#define Encoder2_A_Pin GPIO_PIN_1
#define Encoder2_A_GPIO_Port GPIOB
#define Encoder2_SW_Pin GPIO_PIN_2
#define Encoder2_SW_GPIO_Port GPIOB
#define TFT_DC_Pin GPIO_PIN_6
#define TFT_DC_GPIO_Port GPIOH
#define Encoder1_B_Pin GPIO_PIN_7
#define Encoder1_B_GPIO_Port GPIOH
#define Encoder1_A_Pin GPIO_PIN_8
#define Encoder1_A_GPIO_Port GPIOH
#define Encoder1_SW_Pin GPIO_PIN_9
#define Encoder1_SW_GPIO_Port GPIOH
#define Encoder0_B_Pin GPIO_PIN_10
#define Encoder0_B_GPIO_Port GPIOH
#define Encoder0_A_Pin GPIO_PIN_11
#define Encoder0_A_GPIO_Port GPIOH
#define Encoder0_SW_Pin GPIO_PIN_12
#define Encoder0_SW_GPIO_Port GPIOH
#define SSPI_CS_Pin GPIO_PIN_8
#define SSPI_CS_GPIO_Port GPIOC
#define SSPI_CLK_Pin GPIO_PIN_9
#define SSPI_CLK_GPIO_Port GPIOC
#define SSPI_DATA_Pin GPIO_PIN_8
#define SSPI_DATA_GPIO_Port GPIOA
#define AUDIO_MUTE_Pin GPIO_PIN_9
#define AUDIO_MUTE_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_11
#define LED_GPIO_Port GPIOG
#define FootSwitch2B_Pin GPIO_PIN_12
#define FootSwitch2B_GPIO_Port GPIOG
#define FootSwitch2A_Pin GPIO_PIN_5
#define FootSwitch2A_GPIO_Port GPIOB
#define FootSwitch1B_Pin GPIO_PIN_6
#define FootSwitch1B_GPIO_Port GPIOB
#define FootSwitch1A_Pin GPIO_PIN_7
#define FootSwitch1A_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
