/**
 * @file stc_hal_stm32.h
 * @brief STM32 HAL层实现
 */

#ifndef __STC_HAL_STM32_H__
#define __STC_HAL_STM32_H__

#include "../stc_types.h"
#include "../stc_context.h"

/* STM32 HAL头文件 - 根据实际使用的芯片调整 */
#ifdef STM32G4xx
#include "stm32g4xx_hal.h"
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#else
/* 通用定义，实际使用时需包含正确的头文件 */
typedef void* UART_HandleTypeDef;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STM32 UART句柄封装
 *============================================================================*/
typedef struct {
    UART_HandleTypeDef* huart;      // STM32 HAL UART句柄
    uint8_t             rx_buffer[STC_MAX_PACKET_SIZE];
    volatile uint16_t   rx_head;    // 环形缓冲区头
    volatile uint16_t   rx_tail;    // 环形缓冲区尾
    volatile uint8_t    rx_complete;
} stc_stm32_uart_t;

/*============================================================================
 * API函数
 *============================================================================*/

/**
 * @brief 获取STM32 HAL接口
 * @return HAL接口指针
 */
const stc_hal_t* stc_hal_stm32_get(void);

/**
 * @brief 初始化STM32 UART
 * @param uart UART封装结构
 * @param huart STM32 HAL UART句柄
 * @return STC_OK成功
 */
int stc_hal_stm32_uart_init(stc_stm32_uart_t* uart, UART_HandleTypeDef* huart);

/**
 * @brief 启动接收（使用中断）
 * @param uart UART封装结构
 * @return STC_OK成功
 */
int stc_hal_stm32_uart_start_receive(stc_stm32_uart_t* uart);

/**
 * @brief UART接收完成中断回调（在HAL_UART_RxCpltCallback中调用）
 * @param uart UART封装结构
 * @param data 接收到的数据
 * @param len 数据长度
 */
void stc_hal_stm32_uart_rx_callback(stc_stm32_uart_t* uart, uint8_t* data, uint16_t len);

/**
 * @brief 清空接收缓冲区
 * @param uart UART封装结构
 */
void stc_hal_stm32_uart_flush(stc_stm32_uart_t* uart);

#ifdef __cplusplus
}
#endif

#endif /* __STC_HAL_STM32_H__ */

