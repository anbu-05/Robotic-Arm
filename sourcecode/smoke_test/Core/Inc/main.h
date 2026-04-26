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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void USB_CDC_RxHandler(uint8_t*, uint32_t);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define usr_led_Pin GPIO_PIN_13
#define usr_led_GPIO_Port GPIOC
#define A01_Pin GPIO_PIN_14
#define A01_GPIO_Port GPIOC
#define A02_Pin GPIO_PIN_15
#define A02_GPIO_Port GPIOC
#define key_Pin GPIO_PIN_0
#define key_GPIO_Port GPIOA
#define key_EXTI_IRQn EXTI0_IRQn
#define encA0_Pin GPIO_PIN_1
#define encA0_GPIO_Port GPIOA
#define encB0_Pin GPIO_PIN_2
#define encB0_GPIO_Port GPIOA
#define encA1_Pin GPIO_PIN_3
#define encA1_GPIO_Port GPIOA
#define encB1_Pin GPIO_PIN_4
#define encB1_GPIO_Port GPIOA
#define encA2_Pin GPIO_PIN_5
#define encA2_GPIO_Port GPIOA
#define encB2_Pin GPIO_PIN_6
#define encB2_GPIO_Port GPIOA
#define B21_Pin GPIO_PIN_7
#define B21_GPIO_Port GPIOA
#define B12_Pin GPIO_PIN_0
#define B12_GPIO_Port GPIOB
#define B11_Pin GPIO_PIN_1
#define B11_GPIO_Port GPIOB
#define A12_Pin GPIO_PIN_2
#define A12_GPIO_Port GPIOB
#define A11_Pin GPIO_PIN_10
#define A11_GPIO_Port GPIOB
#define STBY0_Pin GPIO_PIN_12
#define STBY0_GPIO_Port GPIOB
#define STBY1_Pin GPIO_PIN_13
#define STBY1_GPIO_Port GPIOB
#define STBY2_Pin GPIO_PIN_14
#define STBY2_GPIO_Port GPIOB
#define B22_Pin GPIO_PIN_15
#define B22_GPIO_Port GPIOB
#define A21_Pin GPIO_PIN_8
#define A21_GPIO_Port GPIOA
#define A22_Pin GPIO_PIN_9
#define A22_GPIO_Port GPIOA
#define B02_Pin GPIO_PIN_15
#define B02_GPIO_Port GPIOA
#define B01_Pin GPIO_PIN_3
#define B01_GPIO_Port GPIOB
#define PB2_Pin GPIO_PIN_4
#define PB2_GPIO_Port GPIOB
#define PA2_Pin GPIO_PIN_5
#define PA2_GPIO_Port GPIOB
#define PB1_Pin GPIO_PIN_6
#define PB1_GPIO_Port GPIOB
#define PA1_Pin GPIO_PIN_7
#define PA1_GPIO_Port GPIOB
#define PB0_Pin GPIO_PIN_8
#define PB0_GPIO_Port GPIOB
#define PA0_Pin GPIO_PIN_9
#define PA0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
