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
#include "stm32f4xx_hal.h"

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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED4_Pin GPIO_PIN_2
#define LED4_GPIO_Port GPIOE
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOA
#define LED8_Pin GPIO_PIN_3
#define LED8_GPIO_Port GPIOA
#define Light_PA6_Pin GPIO_PIN_6
#define Light_PA6_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_10
#define LED1_GPIO_Port GPIOB
#define LED7_Pin GPIO_PIN_12
#define LED7_GPIO_Port GPIOB
#define SYN_Pin GPIO_PIN_15
#define SYN_GPIO_Port GPIOA
#define DIN_Pin GPIO_PIN_12
#define DIN_GPIO_Port GPIOC
#define CLR_Pin GPIO_PIN_1
#define CLR_GPIO_Port GPIOD
#define LD_Pin GPIO_PIN_2
#define LD_GPIO_Port GPIOD
#define PD3_Pin GPIO_PIN_3
#define PD3_GPIO_Port GPIOD
#define PD4_Pin GPIO_PIN_4
#define PD4_GPIO_Port GPIOD
#define PD5_Pin GPIO_PIN_5
#define PD5_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
