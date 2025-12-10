/**
 ******************************************************************************
 * @file    bsp_stc_uart.h
 * @brief   STC通信串口BSP层头文件
 *          基于USART2实现，用于与STC目标MCU通信
 * @version V1.0.0
 * @date    2025-12-10
 ******************************************************************************
 */

#ifndef __BSP_STC_UART_H__
#define __BSP_STC_UART_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "../Interface/dev_serial.h"
#include "ringbuffer.h"
/* Exported defines ----------------------------------------------------------*/

/* 接收缓冲区大小 */
#ifndef BSP_STC_UART_RX_BUF_SIZE
#define BSP_STC_UART_RX_BUF_SIZE 512
#endif

/* 发送缓冲区大小 */
#ifndef BSP_STC_UART_TX_BUF_SIZE
#define BSP_STC_UART_TX_BUF_SIZE 256
#endif

/* 默认超时时间(ms) */
#define BSP_STC_UART_DEFAULT_TIMEOUT 1000

/* 支持的波特率范围 */
#define BSP_STC_UART_MIN_BAUDRATE 2400
#define BSP_STC_UART_MAX_BAUDRATE 230400

  /* Exported types ------------------------------------------------------------*/

  /* BSP层模块状态枚举 */
  typedef enum
  {
    BSP_STC_UART_STATE_IDLE = 0, /* 空闲状态 */
    BSP_STC_UART_STATE_INIT,     /* 已初始化 */
    BSP_STC_UART_STATE_RUNNING,  /* 运行中 */
    BSP_STC_UART_STATE_ERROR     /* 错误状态 */
  } bsp_stc_uart_state_enum;

  /* BSP层错误码枚举 */
  typedef enum
  {
    BSP_STC_UART_OK = 0,           /* 成功 */
    BSP_STC_UART_ERR_PARAM = -1,   /* 参数错误 */
    BSP_STC_UART_ERR_TIMEOUT = -2, /* 超时 */
    BSP_STC_UART_ERR_BUSY = -3,    /* 忙 */
    BSP_STC_UART_ERR_STATE = -4    /* 状态错误 */
  } bsp_stc_uart_err_enum;

  /* Exported functions prototypes ---------------------------------------------*/

  /* 初始化/反初始化 */
  void bsp_stc_uart_init(uint32_t baudrate);
  void bsp_stc_uart_deinit(void);

  /* 数据收发 - 阻塞方式 */
  int bsp_stc_uart_send(const uint8_t *data, uint16_t len);
  int bsp_stc_uart_read(uint8_t *data, uint16_t max_len, uint16_t *actual_len);

  /* 数据收发 - 带超时 */
  int bsp_stc_uart_send_timeout(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
  int bsp_stc_uart_read_timeout(uint8_t *data, uint16_t max_len, uint16_t *actual_len, uint32_t timeout_ms);

  /* 配置修改 */
  int bsp_stc_uart_set_baudrate(uint32_t baudrate);
  int bsp_stc_uart_set_parity(serial_parity_t parity);

  /* 缓冲区管理 */
  uint16_t bsp_stc_uart_get_rx_count(void);
  void bsp_stc_uart_flush_rx(void);
  void bsp_stc_uart_flush_tx(void);

  /* 状态查询 */
  bsp_stc_uart_state_enum bsp_stc_uart_get_state(void);
  uint32_t bsp_stc_uart_get_baudrate(void);
  serial_parity_t bsp_stc_uart_get_parity(void);

  /* 获取设备实例（符合统一设备接口） */
  serial_dev_t *bsp_stc_uart_get_device(void);

  /* 中断回调（由stm32g4xx_it.c调用） */
  void bsp_stc_uart_rx_callback(uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_STC_UART_H__ */
