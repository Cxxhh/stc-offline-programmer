/**
  ******************************************************************************
  * @file    port_def.h
  * @brief   平台移植层公共接口定义
  *          定义所有平台无关的抽象接口
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

#ifndef __PORT_DEF_H__
#define __PORT_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/* 移植层错误码 */
typedef enum {
    PORT_OK = 0,
    PORT_ERROR = -1,
    PORT_TIMEOUT = -2,
    PORT_BUSY = -3,
    PORT_INVALID = -4
} port_status_t;

/* 串口校验位 */
typedef enum {
    PORT_PARITY_NONE = 0,
    PORT_PARITY_EVEN = 1,
    PORT_PARITY_ODD = 2
} port_parity_t;

/* Exported defines ----------------------------------------------------------*/

/* 默认超时时间 */
#define PORT_DEFAULT_TIMEOUT_MS     1000

/* Exported functions prototypes ---------------------------------------------*/

/*
 * ============================================================================
 * UART 接口
 * ============================================================================
 */

/**
 * @brief 初始化UART
 * @param baudrate 波特率
 * @param parity 校验位
 * @return PORT_OK: 成功，其他: 失败
 */
int port_uart_init(uint32_t baudrate, port_parity_t parity);

/**
 * @brief 反初始化UART
 * @return PORT_OK: 成功
 */
int port_uart_deinit(void);

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回发送的字节数，失败返回负数
 */
int port_uart_send(const uint8_t *data, uint16_t len);

/**
 * @brief 发送数据（带超时）
 * @param data 数据指针
 * @param len 数据长度
 * @param timeout_ms 超时时间(ms)
 * @return 成功返回发送的字节数，失败返回负数
 */
int port_uart_send_timeout(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 接收数据
 * @param data 数据缓冲区
 * @param max_len 最大接收长度
 * @param actual_len 实际接收长度（输出）
 * @return PORT_OK: 成功，其他: 失败
 */
int port_uart_recv(uint8_t *data, uint16_t max_len, uint16_t *actual_len);

/**
 * @brief 接收数据（带超时）
 * @param data 数据缓冲区
 * @param max_len 最大接收长度
 * @param actual_len 实际接收长度（输出）
 * @param timeout_ms 超时时间(ms)
 * @return PORT_OK: 成功，PORT_TIMEOUT: 超时，其他: 失败
 */
int port_uart_recv_timeout(uint8_t *data, uint16_t max_len, uint16_t *actual_len, uint32_t timeout_ms);

/**
 * @brief 设置波特率
 * @param baudrate 波特率
 * @return PORT_OK: 成功，其他: 失败
 */
int port_uart_set_baudrate(uint32_t baudrate);

/**
 * @brief 设置校验位
 * @param parity 校验位
 * @return PORT_OK: 成功，其他: 失败
 */
int port_uart_set_parity(port_parity_t parity);

/**
 * @brief 获取接收缓冲区数据量
 * @return 可读字节数
 */
uint16_t port_uart_get_rx_count(void);

/**
 * @brief 清空接收缓冲区
 */
void port_uart_flush_rx(void);

/**
 * @brief 清空发送缓冲区
 */
void port_uart_flush_tx(void);

/*
 * ============================================================================
 * 系统接口
 * ============================================================================
 */

/**
 * @brief 获取系统时间戳（毫秒）
 * @return 当前时间戳(ms)
 */
uint32_t port_get_tick(void);

/**
 * @brief 毫秒延时
 * @param ms 延时毫秒数
 */
void port_delay_ms(uint32_t ms);

/**
 * @brief 微秒延时（精度可能受限）
 * @param us 延时微秒数
 */
void port_delay_us(uint32_t us);

/**
 * @brief 进入临界区（禁用中断）
 */
void port_enter_critical(void);

/**
 * @brief 退出临界区（恢复中断）
 */
void port_exit_critical(void);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_DEF_H__ */

