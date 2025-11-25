/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_uart_adapter.h
  * @brief   UART日志输出适配器头文件
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LOG_UART_ADAPTER_H__
#define __LOG_UART_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "log.h"

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
/**
 * @brief 初始化UART日志输出适配器（原始数据输出方式，需要先sprintf格式化）
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_init(void);

/**
 * @brief 初始化UART日志输出适配器（printf格式输出方式）
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_init_printf(void);

/**
 * @brief 注销UART日志输出适配器
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_deinit(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __LOG_UART_ADAPTER_H__ */

