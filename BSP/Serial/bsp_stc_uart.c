/**
  ******************************************************************************
  * @file    bsp_stc_uart.c
  * @brief   STC通信串口BSP层实现
  *          基于USART2实现，集成ringbuffer用于接收缓冲
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp_stc_uart.h"
#include "../../Service/ringbuffer.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* 接收缓冲区 */
static uint8_t s_rx_buffer_data[BSP_STC_UART_RX_BUF_SIZE];
static ringbuffer_t *s_p_rx_buffer = NULL;

/* 模块状态 */
static bsp_stc_uart_state_enum s_state = BSP_STC_UART_STATE_IDLE;

/* 当前配置 */
static uint32_t s_current_baudrate = 2400;
static serial_parity_t s_current_parity = SERIAL_PARITY_EVEN;

/* 串口设备实例 */
static serial_dev_t s_serial_dev;

/* Private function prototypes -----------------------------------------------*/
static dev_status_t _dev_open(dev_base_t *dev);
static dev_status_t _dev_close(dev_base_t *dev);
static dev_status_t _dev_ioctl(dev_base_t *dev, uint32_t cmd, void *arg);
static dev_status_t _dev_write(serial_dev_t *dev, const uint8_t *data, uint16_t len);
static dev_status_t _dev_read(serial_dev_t *dev, uint8_t *data, uint16_t len, uint16_t *actual);
static dev_status_t _dev_set_config(serial_dev_t *dev, const serial_config_t *config);
static dev_status_t _dev_get_config(serial_dev_t *dev, serial_config_t *config);
static uint16_t _dev_get_rx_count(serial_dev_t *dev);
static void _dev_flush_rx(serial_dev_t *dev);
static void _dev_flush_tx(serial_dev_t *dev);

static void _usart2_enable(void);
static void _usart2_disable(void);
static void _usart2_wait_ready(void);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化STC通信串口
 * @param baudrate 波特率
 */
void bsp_stc_uart_init(uint32_t baudrate)
{
    /* 初始化环形缓冲区 */
    s_p_rx_buffer = ringbuffer_register(s_rx_buffer_data, sizeof(s_rx_buffer_data));
    if (s_p_rx_buffer == NULL)
    {
        s_state = BSP_STC_UART_STATE_ERROR;
        return;
    }

    /* 保存配置 */
    s_current_baudrate = baudrate;

    /* 如果波特率不是默认的2400，需要重新配置 */
    if (baudrate != 2400)
    {
        bsp_stc_uart_set_baudrate(baudrate);
    }

    /* 使能接收中断 */
    LL_USART_EnableIT_RXNE(USART2);

    /* 初始化设备结构 */
    s_serial_dev.base.name = "stc_uart";
    s_serial_dev.base.type = DEV_TYPE_SERIAL;
    s_serial_dev.base.is_open = false;
    s_serial_dev.base.priv_data = NULL;
    s_serial_dev.base.open = _dev_open;
    s_serial_dev.base.close = _dev_close;
    s_serial_dev.base.ioctl = _dev_ioctl;

    s_serial_dev.write = _dev_write;
    s_serial_dev.read = _dev_read;
    s_serial_dev.set_config = _dev_set_config;
    s_serial_dev.get_config = _dev_get_config;
    s_serial_dev.get_rx_count = _dev_get_rx_count;
    s_serial_dev.flush_rx = _dev_flush_rx;
    s_serial_dev.flush_tx = _dev_flush_tx;
    s_serial_dev.rx_callback = NULL;
    s_serial_dev.tx_complete_callback = NULL;

    s_serial_dev.config.baudrate = s_current_baudrate;
    s_serial_dev.config.databits = 8;
    s_serial_dev.config.stopbits = 1;
    s_serial_dev.config.parity = s_current_parity;
    s_serial_dev.timeout_ms = BSP_STC_UART_DEFAULT_TIMEOUT;

    s_state = BSP_STC_UART_STATE_INIT;
}

/**
 * @brief 反初始化STC通信串口
 */
void bsp_stc_uart_deinit(void)
{
    /* 禁用接收中断 */
    LL_USART_DisableIT_RXNE(USART2);

    /* 注销环形缓冲区 */
    if (s_p_rx_buffer != NULL)
    {
        ringbuffer_unregister(s_p_rx_buffer);
        s_p_rx_buffer = NULL;
    }

    s_state = BSP_STC_UART_STATE_IDLE;
}

/**
 * @brief 发送数据（阻塞方式）
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回发送的字节数，失败返回负数
 */
int bsp_stc_uart_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return BSP_STC_UART_ERR_PARAM;
    }

    if (s_state != BSP_STC_UART_STATE_INIT && s_state != BSP_STC_UART_STATE_RUNNING)
    {
        return BSP_STC_UART_ERR_STATE;
    }

    s_state = BSP_STC_UART_STATE_RUNNING;

    for (uint16_t i = 0; i < len; i++)
    {
        /* 等待发送寄存器为空 */
        while (!LL_USART_IsActiveFlag_TXE(USART2))
        {
            /* 等待 */
        }
        LL_USART_TransmitData8(USART2, data[i]);
    }

    /* 等待发送完成 */
    while (!LL_USART_IsActiveFlag_TC(USART2))
    {
        /* 等待 */
    }

    return (int)len;
}

/**
 * @brief 发送数据（带超时）
 * @param data 数据指针
 * @param len 数据长度
 * @param timeout_ms 超时时间(ms)
 * @return 成功返回发送的字节数，失败返回负数
 */
int bsp_stc_uart_send_timeout(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (data == NULL || len == 0)
    {
        return BSP_STC_UART_ERR_PARAM;
    }

    if (s_state != BSP_STC_UART_STATE_INIT && s_state != BSP_STC_UART_STATE_RUNNING)
    {
        return BSP_STC_UART_ERR_STATE;
    }

    s_state = BSP_STC_UART_STATE_RUNNING;

    uint32_t start_tick = HAL_GetTick();

    for (uint16_t i = 0; i < len; i++)
    {
        /* 等待发送寄存器为空，带超时 */
        while (!LL_USART_IsActiveFlag_TXE(USART2))
        {
            if ((HAL_GetTick() - start_tick) >= timeout_ms)
            {
                return BSP_STC_UART_ERR_TIMEOUT;
            }
        }
        LL_USART_TransmitData8(USART2, data[i]);
    }

    /* 等待发送完成，带超时 */
    while (!LL_USART_IsActiveFlag_TC(USART2))
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            return BSP_STC_UART_ERR_TIMEOUT;
        }
    }

    return (int)len;
}

/**
 * @brief 读取数据（阻塞方式）
 * @param data 数据缓冲区
 * @param max_len 最大读取长度
 * @param actual_len 实际读取长度（输出）
 * @return 成功返回0，失败返回负数
 */
int bsp_stc_uart_read(uint8_t *data, uint16_t max_len, uint16_t *actual_len)
{
    if (data == NULL || max_len == 0)
    {
        return BSP_STC_UART_ERR_PARAM;
    }

    if (s_p_rx_buffer == NULL)
    {
        return BSP_STC_UART_ERR_STATE;
    }

    uint16_t read_count = s_p_rx_buffer->read(s_p_rx_buffer, data, max_len);

    if (actual_len != NULL)
    {
        *actual_len = read_count;
    }

    return BSP_STC_UART_OK;
}

/**
 * @brief 读取数据（带超时）
 * @param data 数据缓冲区
 * @param max_len 最大读取长度
 * @param actual_len 实际读取长度（输出）
 * @param timeout_ms 超时时间(ms)
 * @return 成功返回0，失败返回负数
 */
int bsp_stc_uart_read_timeout(uint8_t *data, uint16_t max_len, uint16_t *actual_len, uint32_t timeout_ms)
{
    if (data == NULL || max_len == 0)
    {
        return BSP_STC_UART_ERR_PARAM;
    }

    if (s_p_rx_buffer == NULL)
    {
        return BSP_STC_UART_ERR_STATE;
    }

    uint32_t start_tick = HAL_GetTick();
    uint16_t total_read = 0;

    /* 等待至少有一个字节或超时 */
    while (s_p_rx_buffer->is_empty(s_p_rx_buffer))
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            if (actual_len != NULL)
            {
                *actual_len = 0;
            }
            return BSP_STC_UART_ERR_TIMEOUT;
        }
    }

    /* 读取所有可用数据 */
    total_read = s_p_rx_buffer->read(s_p_rx_buffer, data, max_len);

    if (actual_len != NULL)
    {
        *actual_len = total_read;
    }

    return BSP_STC_UART_OK;
}

/**
 * @brief 设置波特率
 * @param baudrate 波特率值
 * @return 成功返回0，失败返回负数
 */
int bsp_stc_uart_set_baudrate(uint32_t baudrate)
{
    if (baudrate < BSP_STC_UART_MIN_BAUDRATE || baudrate > BSP_STC_UART_MAX_BAUDRATE)
    {
        return BSP_STC_UART_ERR_PARAM;
    }

    /* 禁用USART */
    _usart2_disable();

    /* 获取时钟频率并设置波特率 */
    LL_USART_SetBaudRate(USART2,
                         LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE),
                         LL_USART_PRESCALER_DIV1,
                         LL_USART_OVERSAMPLING_16,
                         baudrate);

    /* 重新使能USART */
    _usart2_enable();
    _usart2_wait_ready();

    s_current_baudrate = baudrate;
    s_serial_dev.config.baudrate = baudrate;

    return BSP_STC_UART_OK;
}

/**
 * @brief 设置校验位
 * @param parity 校验位类型
 * @return 成功返回0，失败返回负数
 */
int bsp_stc_uart_set_parity(serial_parity_t parity)
{
    /* 禁用USART */
    _usart2_disable();

    switch (parity)
    {
    case SERIAL_PARITY_NONE:
        /* 8N1模式 - STC89/10/11系列 */
        LL_USART_SetDataWidth(USART2, LL_USART_DATAWIDTH_8B);
        LL_USART_SetParity(USART2, LL_USART_PARITY_NONE);
        break;

    case SERIAL_PARITY_EVEN:
        /* 8E1模式 - STC8/12/15系列 */
        LL_USART_SetDataWidth(USART2, LL_USART_DATAWIDTH_9B);
        LL_USART_SetParity(USART2, LL_USART_PARITY_EVEN);
        break;

    case SERIAL_PARITY_ODD:
        /* 8O1模式 */
        LL_USART_SetDataWidth(USART2, LL_USART_DATAWIDTH_9B);
        LL_USART_SetParity(USART2, LL_USART_PARITY_ODD);
        break;

    default:
        _usart2_enable();
        return BSP_STC_UART_ERR_PARAM;
    }

    /* 重新使能USART */
    _usart2_enable();
    _usart2_wait_ready();

    s_current_parity = parity;
    s_serial_dev.config.parity = parity;

    return BSP_STC_UART_OK;
}

/**
 * @brief 获取接收缓冲区数据量
 * @return 可读字节数
 */
uint16_t bsp_stc_uart_get_rx_count(void)
{
    if (s_p_rx_buffer == NULL)
    {
        return 0;
    }
    return s_p_rx_buffer->get_count(s_p_rx_buffer);
}

/**
 * @brief 清空接收缓冲区
 */
void bsp_stc_uart_flush_rx(void)
{
    if (s_p_rx_buffer != NULL)
    {
        s_p_rx_buffer->clear(s_p_rx_buffer);
    }
}

/**
 * @brief 清空发送缓冲区（等待发送完成）
 */
void bsp_stc_uart_flush_tx(void)
{
    while (!LL_USART_IsActiveFlag_TC(USART2))
    {
        /* 等待发送完成 */
    }
}

/**
 * @brief 获取模块状态
 * @return 当前状态
 */
bsp_stc_uart_state_enum bsp_stc_uart_get_state(void)
{
    return s_state;
}

/**
 * @brief 获取当前波特率
 * @return 波特率值
 */
uint32_t bsp_stc_uart_get_baudrate(void)
{
    return s_current_baudrate;
}

/**
 * @brief 获取当前校验位
 * @return 校验位类型
 */
serial_parity_t bsp_stc_uart_get_parity(void)
{
    return s_current_parity;
}

/**
 * @brief 获取串口设备实例
 * @return 设备指针
 */
serial_dev_t *bsp_stc_uart_get_device(void)
{
    return &s_serial_dev;
}

/**
 * @brief 串口接收中断回调（由stm32g4xx_it.c调用）
 * @param data 接收到的数据
 */
void bsp_stc_uart_rx_callback(uint8_t data)
{
    if (s_p_rx_buffer != NULL)
    {
        s_p_rx_buffer->write_byte(s_p_rx_buffer, data);
    }

    /* 如果注册了回调函数，调用它 */
    if (s_serial_dev.rx_callback != NULL)
    {
        s_serial_dev.rx_callback(&s_serial_dev, data);
    }
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 设备打开
 */
static dev_status_t _dev_open(dev_base_t *dev)
{
    if (dev == NULL)
    {
        return DEV_INVALID;
    }

    if (dev->is_open)
    {
        return DEV_OK;
    }

    /* 使能接收中断 */
    LL_USART_EnableIT_RXNE(USART2);

    dev->is_open = true;
    s_state = BSP_STC_UART_STATE_RUNNING;

    return DEV_OK;
}

/**
 * @brief 设备关闭
 */
static dev_status_t _dev_close(dev_base_t *dev)
{
    if (dev == NULL)
    {
        return DEV_INVALID;
    }

    if (!dev->is_open)
    {
        return DEV_OK;
    }

    /* 禁用接收中断 */
    LL_USART_DisableIT_RXNE(USART2);

    dev->is_open = false;
    s_state = BSP_STC_UART_STATE_INIT;

    return DEV_OK;
}

/**
 * @brief 设备控制
 */
static dev_status_t _dev_ioctl(dev_base_t *dev, uint32_t cmd, void *arg)
{
    if (dev == NULL)
    {
        return DEV_INVALID;
    }

    switch (cmd)
    {
    case SERIAL_IOCTL_SET_BAUDRATE:
        return (bsp_stc_uart_set_baudrate((uint32_t)(uintptr_t)arg) == 0) ? DEV_OK : DEV_ERROR;

    case SERIAL_IOCTL_SET_PARITY:
        return (bsp_stc_uart_set_parity((serial_parity_t)(uintptr_t)arg) == 0) ? DEV_OK : DEV_ERROR;

    case SERIAL_IOCTL_GET_RX_COUNT:
        if (arg != NULL)
        {
            *(uint16_t *)arg = bsp_stc_uart_get_rx_count();
            return DEV_OK;
        }
        return DEV_INVALID;

    case SERIAL_IOCTL_FLUSH_RX:
        bsp_stc_uart_flush_rx();
        return DEV_OK;

    case SERIAL_IOCTL_FLUSH_TX:
        bsp_stc_uart_flush_tx();
        return DEV_OK;

    case DEV_IOCTL_RESET:
        bsp_stc_uart_flush_rx();
        bsp_stc_uart_flush_tx();
        return DEV_OK;

    default:
        return DEV_ERROR;
    }
}

/**
 * @brief 设备写入
 */
static dev_status_t _dev_write(serial_dev_t *dev, const uint8_t *data, uint16_t len)
{
    (void)dev;
    int ret = bsp_stc_uart_send(data, len);
    return (ret >= 0) ? DEV_OK : DEV_ERROR;
}

/**
 * @brief 设备读取
 */
static dev_status_t _dev_read(serial_dev_t *dev, uint8_t *data, uint16_t len, uint16_t *actual)
{
    int ret;
    if (dev->timeout_ms > 0)
    {
        ret = bsp_stc_uart_read_timeout(data, len, actual, dev->timeout_ms);
    }
    else
    {
        ret = bsp_stc_uart_read(data, len, actual);
    }
    return (ret == 0) ? DEV_OK : ((ret == BSP_STC_UART_ERR_TIMEOUT) ? DEV_TIMEOUT : DEV_ERROR);
}

/**
 * @brief 设置配置
 */
static dev_status_t _dev_set_config(serial_dev_t *dev, const serial_config_t *config)
{
    if (dev == NULL || config == NULL)
    {
        return DEV_INVALID;
    }

    int ret = bsp_stc_uart_set_baudrate(config->baudrate);
    if (ret != 0)
    {
        return DEV_ERROR;
    }

    ret = bsp_stc_uart_set_parity(config->parity);
    if (ret != 0)
    {
        return DEV_ERROR;
    }

    return DEV_OK;
}

/**
 * @brief 获取配置
 */
static dev_status_t _dev_get_config(serial_dev_t *dev, serial_config_t *config)
{
    if (dev == NULL || config == NULL)
    {
        return DEV_INVALID;
    }

    config->baudrate = s_current_baudrate;
    config->databits = 8;
    config->stopbits = 1;
    config->parity = s_current_parity;

    return DEV_OK;
}

/**
 * @brief 获取接收数据量
 */
static uint16_t _dev_get_rx_count(serial_dev_t *dev)
{
    (void)dev;
    return bsp_stc_uart_get_rx_count();
}

/**
 * @brief 清空接收缓冲区
 */
static void _dev_flush_rx(serial_dev_t *dev)
{
    (void)dev;
    bsp_stc_uart_flush_rx();
}

/**
 * @brief 清空发送缓冲区
 */
static void _dev_flush_tx(serial_dev_t *dev)
{
    (void)dev;
    bsp_stc_uart_flush_tx();
}

/**
 * @brief 使能USART2
 */
static void _usart2_enable(void)
{
    LL_USART_Enable(USART2);
}

/**
 * @brief 禁用USART2
 */
static void _usart2_disable(void)
{
    LL_USART_Disable(USART2);
}

/**
 * @brief 等待USART2就绪
 */
static void _usart2_wait_ready(void)
{
    while (!(LL_USART_IsActiveFlag_TEACK(USART2) && LL_USART_IsActiveFlag_REACK(USART2)))
    {
        /* 等待 */
    }
}

