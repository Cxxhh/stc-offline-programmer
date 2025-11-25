/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    log_uart_adapter.c
  * @brief   UART日志输出适配器示例
  *          演示如何将日志输出到UART
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "log.h"
#include "main.h"
#include "stm32g4xx_ll_usart.h"
#include <stdio.h>
#include <stdarg.h>

/* USER CODE BEGIN Includes */

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
static log_output_handle_t s_uart_log_handle;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static int log_uart_output_raw(const uint8_t *data, uint32_t len);
static int log_uart_output_printf(const char *format, va_list args);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化UART日志输出适配器（原始数据输出方式）
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_init(void)
{
    // 设置输出类型和函数
    s_uart_log_handle.output_type = LOG_OUTPUT_TYPE_RAW;
    s_uart_log_handle.output_func.raw_func = log_uart_output_raw;
    s_uart_log_handle.user_data = NULL;

    // 注册到日志服务（原始数据输出，需要先sprintf格式化）
    return log_register_output(&s_uart_log_handle);
}

/**
 * @brief 初始化UART日志输出适配器（printf格式输出方式）
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_init_printf(void)
{
    // 设置输出类型和函数
    s_uart_log_handle.output_type = LOG_OUTPUT_TYPE_PRINTF;
    s_uart_log_handle.output_func.printf_func = log_uart_output_printf;
    s_uart_log_handle.user_data = NULL;

    // 注册到日志服务（printf格式输出）
    return log_register_output_printf(&s_uart_log_handle);
}

/**
 * @brief 注销UART日志输出适配器
 * @return 成功返回0，失败返回非0
 */
int log_uart_adapter_deinit(void)
{
    return log_unregister_output(&s_uart_log_handle);
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief UART原始数据输出函数（非printf格式）
 * @param data 要输出的数据（已格式化的字符串）
 * @param len 数据长度
 * @return 成功返回0，失败返回非0
 */
static int log_uart_output_raw(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0)
    {
        return -1;
    }

    // 通过USART1输出数据
    for (uint32_t i = 0; i < len; i++)
    {
        // 等待发送寄存器为空
        while (!LL_USART_IsActiveFlag_TXE(USART1))
        {
            // 等待发送就绪
        }

        // 发送数据
        LL_USART_TransmitData8(USART1, data[i]);
    }

    // 等待发送完成
    while (!LL_USART_IsActiveFlag_TC(USART1))
    {
        // 等待传输完成
    }

    return 0;
}

/**
 * @brief UART printf格式输出函数
 * @param format 格式化字符串（已包含级别前缀）
 * @param args 可变参数列表
 * @return 成功返回0，失败返回非0
 */
static int log_uart_output_printf(const char *format, va_list args)
{
    if (format == NULL)
    {
        return -1;
    }

    // 格式化字符串到缓冲区
    char buffer[LOG_BUFFER_SIZE];
    int len = vsnprintf(buffer, LOG_BUFFER_SIZE, format, args);
    
    if (len < 0 || len >= LOG_BUFFER_SIZE)
    {
        return -2;  // 格式化失败或缓冲区不足
    }

    // 通过USART1输出数据
    for (int i = 0; i < len; i++)
    {
        // 等待发送寄存器为空
        while (!LL_USART_IsActiveFlag_TXE(USART1))
        {
            // 等待发送就绪
        }

        // 发送数据
        LL_USART_TransmitData8(USART1, (uint8_t)buffer[i]);
    }

    // 等待发送完成
    while (!LL_USART_IsActiveFlag_TC(USART1))
    {
        // 等待传输完成
    }

    return 0;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

