/**
  ******************************************************************************
  * @file    port_uart.c
  * @brief   STM32G4平台UART移植层实现
  *          基于BSP层的bsp_stc_uart实现
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "../port_def.h"
#include "../../BSP/Serial/bsp_stc_uart.h"
#include "../../BSP/Interface/dev_serial.h"

/* Private variables ---------------------------------------------------------*/

static bool s_is_initialized = false;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化UART
 */
int port_uart_init(uint32_t baudrate, port_parity_t parity)
{
    /* 初始化BSP层 */
    bsp_stc_uart_init(baudrate);

    /* 设置校验位 */
    serial_parity_t serial_parity;
    switch (parity)
    {
    case PORT_PARITY_NONE:
        serial_parity = SERIAL_PARITY_NONE;
        break;
    case PORT_PARITY_EVEN:
        serial_parity = SERIAL_PARITY_EVEN;
        break;
    case PORT_PARITY_ODD:
        serial_parity = SERIAL_PARITY_ODD;
        break;
    default:
        serial_parity = SERIAL_PARITY_EVEN;
        break;
    }

    int ret = bsp_stc_uart_set_parity(serial_parity);
    if (ret != 0)
    {
        return PORT_ERROR;
    }

    s_is_initialized = true;
    return PORT_OK;
}

/**
 * @brief 反初始化UART
 */
int port_uart_deinit(void)
{
    bsp_stc_uart_deinit();
    s_is_initialized = false;
    return PORT_OK;
}

/**
 * @brief 发送数据
 */
int port_uart_send(const uint8_t *data, uint16_t len)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }
    return bsp_stc_uart_send(data, len);
}

/**
 * @brief 发送数据（带超时）
 */
int port_uart_send_timeout(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }
    return bsp_stc_uart_send_timeout(data, len, timeout_ms);
}

/**
 * @brief 接收数据
 */
int port_uart_recv(uint8_t *data, uint16_t max_len, uint16_t *actual_len)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }

    int ret = bsp_stc_uart_read(data, max_len, actual_len);
    if (ret == BSP_STC_UART_OK)
    {
        return PORT_OK;
    }
    return PORT_ERROR;
}

/**
 * @brief 接收数据（带超时）
 */
int port_uart_recv_timeout(uint8_t *data, uint16_t max_len, uint16_t *actual_len, uint32_t timeout_ms)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }

    int ret = bsp_stc_uart_read_timeout(data, max_len, actual_len, timeout_ms);
    switch (ret)
    {
    case BSP_STC_UART_OK:
        return PORT_OK;
    case BSP_STC_UART_ERR_TIMEOUT:
        return PORT_TIMEOUT;
    default:
        return PORT_ERROR;
    }
}

/**
 * @brief 设置波特率
 */
int port_uart_set_baudrate(uint32_t baudrate)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }

    int ret = bsp_stc_uart_set_baudrate(baudrate);
    return (ret == 0) ? PORT_OK : PORT_ERROR;
}

/**
 * @brief 设置校验位
 */
int port_uart_set_parity(port_parity_t parity)
{
    if (!s_is_initialized)
    {
        return PORT_ERROR;
    }

    serial_parity_t serial_parity;
    switch (parity)
    {
    case PORT_PARITY_NONE:
        serial_parity = SERIAL_PARITY_NONE;
        break;
    case PORT_PARITY_EVEN:
        serial_parity = SERIAL_PARITY_EVEN;
        break;
    case PORT_PARITY_ODD:
        serial_parity = SERIAL_PARITY_ODD;
        break;
    default:
        return PORT_INVALID;
    }

    int ret = bsp_stc_uart_set_parity(serial_parity);
    return (ret == 0) ? PORT_OK : PORT_ERROR;
}

/**
 * @brief 获取接收缓冲区数据量
 */
uint16_t port_uart_get_rx_count(void)
{
    if (!s_is_initialized)
    {
        return 0;
    }
    return bsp_stc_uart_get_rx_count();
}

/**
 * @brief 清空接收缓冲区
 */
void port_uart_flush_rx(void)
{
    if (s_is_initialized)
    {
        bsp_stc_uart_flush_rx();
    }
}

/**
 * @brief 清空发送缓冲区
 */
void port_uart_flush_tx(void)
{
    if (s_is_initialized)
    {
        bsp_stc_uart_flush_tx();
    }
}

