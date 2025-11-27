/**
 * @file stc_hal_stm32.c
 * @brief STM32 HAL层实现
 */

#include "stc_hal_stm32.h"
#include <string.h>

/*============================================================================
 * 内部函数声明
 *============================================================================*/
static int hal_set_baudrate(void* handle, uint32_t baudrate);
static int hal_set_parity(void* handle, stc_parity_t parity);
static int hal_write(void* handle, const uint8_t* data, uint16_t len, uint32_t timeout_ms);
static int hal_read(void* handle, uint8_t* data, uint16_t max_len, uint32_t timeout_ms);
static void hal_flush(void* handle);
static void hal_delay_ms(uint32_t ms);
static uint32_t hal_get_tick_ms(void);

/*============================================================================
 * HAL接口实例
 *============================================================================*/
static const stc_hal_t g_stm32_hal = {
    .set_baudrate = hal_set_baudrate,
    .set_parity = hal_set_parity,
    .write = hal_write,
    .read = hal_read,
    .flush = hal_flush,
    .delay_ms = hal_delay_ms,
    .get_tick_ms = hal_get_tick_ms,
};

const stc_hal_t* stc_hal_stm32_get(void)
{
    return &g_stm32_hal;
}

/*============================================================================
 * HAL函数实现
 *============================================================================*/

static int hal_set_baudrate(void* handle, uint32_t baudrate)
{
    stc_stm32_uart_t* uart = (stc_stm32_uart_t*)handle;
    if (uart == NULL || uart->huart == NULL) {
        return -1;
    }
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    /* 停止UART */
    HAL_UART_DeInit(uart->huart);
    
    /* 修改波特率 */
    uart->huart->Init.BaudRate = baudrate;
    
    /* 重新初始化 */
    if (HAL_UART_Init(uart->huart) != HAL_OK) {
        return -1;
    }
    
    /* 重新启动接收 */
    stc_hal_stm32_uart_start_receive(uart);
#endif
    
    return 0;
}

static int hal_set_parity(void* handle, stc_parity_t parity)
{
    stc_stm32_uart_t* uart = (stc_stm32_uart_t*)handle;
    if (uart == NULL || uart->huart == NULL) {
        return -1;
    }
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    /* 停止UART */
    HAL_UART_DeInit(uart->huart);
    
    /* 修改校验位 */
    if (parity == STC_PARITY_EVEN) {
        uart->huart->Init.Parity = UART_PARITY_EVEN;
        uart->huart->Init.WordLength = UART_WORDLENGTH_9B;  /* 8数据位+1校验位 */
    } else {
        uart->huart->Init.Parity = UART_PARITY_NONE;
        uart->huart->Init.WordLength = UART_WORDLENGTH_8B;
    }
    
    /* 重新初始化 */
    if (HAL_UART_Init(uart->huart) != HAL_OK) {
        return -1;
    }
    
    /* 重新启动接收 */
    stc_hal_stm32_uart_start_receive(uart);
#endif
    
    return 0;
}

static int hal_write(void* handle, const uint8_t* data, uint16_t len, uint32_t timeout_ms)
{
    stc_stm32_uart_t* uart = (stc_stm32_uart_t*)handle;
    if (uart == NULL || uart->huart == NULL || data == NULL) {
        return -1;
    }
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    HAL_StatusTypeDef status = HAL_UART_Transmit(uart->huart, (uint8_t*)data, len, timeout_ms);
    if (status != HAL_OK) {
        return -1;
    }
#endif
    
    return len;
}

static int hal_read(void* handle, uint8_t* data, uint16_t max_len, uint32_t timeout_ms)
{
    stc_stm32_uart_t* uart = (stc_stm32_uart_t*)handle;
    if (uart == NULL || uart->huart == NULL || data == NULL) {
        return -1;
    }
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    /* 使用环形缓冲区读取 */
    uint32_t start_tick = HAL_GetTick();
    uint16_t read_count = 0;
    
    while (read_count < max_len) {
        /* 检查是否有数据 */
        if (uart->rx_head != uart->rx_tail) {
            data[read_count++] = uart->rx_buffer[uart->rx_tail];
            uart->rx_tail = (uart->rx_tail + 1) % sizeof(uart->rx_buffer);
        } else {
            /* 检查超时 */
            if ((HAL_GetTick() - start_tick) >= timeout_ms) {
                break;
            }
            
            /* 如果已读取部分数据，再等待一小段时间 */
            if (read_count > 0) {
                HAL_Delay(10);
                if (uart->rx_head == uart->rx_tail) {
                    break;  /* 没有更多数据 */
                }
            }
        }
    }
    
    return (read_count > 0) ? read_count : -1;
    
#else
    /* 非STM32平台的简单实现 */
    (void)timeout_ms;
    return -1;
#endif
}

static void hal_flush(void* handle)
{
    stc_stm32_uart_t* uart = (stc_stm32_uart_t*)handle;
    if (uart == NULL) {
        return;
    }
    
    /* 清空环形缓冲区 */
    uart->rx_head = 0;
    uart->rx_tail = 0;
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    /* 清空HAL接收缓冲区 */
    __HAL_UART_FLUSH_DRREGISTER(uart->huart);
#endif
}

static void hal_delay_ms(uint32_t ms)
{
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    HAL_Delay(ms);
#else
    /* 简单延时实现 */
    volatile uint32_t count = ms * 1000;
    while (count--);
#endif
}

static uint32_t hal_get_tick_ms(void)
{
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    return HAL_GetTick();
#else
    static uint32_t tick = 0;
    return tick++;
#endif
}

/*============================================================================
 * 公共API实现
 *============================================================================*/

int stc_hal_stm32_uart_init(stc_stm32_uart_t* uart, UART_HandleTypeDef* huart)
{
    if (uart == NULL || huart == NULL) {
        return -1;
    }
    
    memset(uart, 0, sizeof(stc_stm32_uart_t));
    uart->huart = huart;
    uart->rx_head = 0;
    uart->rx_tail = 0;
    uart->rx_complete = 0;
    
    return 0;
}

int stc_hal_stm32_uart_start_receive(stc_stm32_uart_t* uart)
{
    if (uart == NULL || uart->huart == NULL) {
        return -1;
    }
    
#if defined(STM32G4xx) || defined(STM32F4xx) || defined(STM32F1xx)
    /* 使用空闲中断接收 */
    /* 注意：需要在STM32CubeMX中启用UART空闲中断 */
    /* 或者使用DMA接收 */
    
    /* 简单模式：使用HAL_UART_Receive_IT逐字节接收 */
    /* 实际项目中建议使用DMA+空闲中断 */
    
    /* 清空缓冲区 */
    uart->rx_head = 0;
    uart->rx_tail = 0;
#endif
    
    return 0;
}

void stc_hal_stm32_uart_rx_callback(stc_stm32_uart_t* uart, uint8_t* data, uint16_t len)
{
    if (uart == NULL || data == NULL) {
        return;
    }
    
    /* 将接收到的数据存入环形缓冲区 */
    for (uint16_t i = 0; i < len; i++) {
        uint16_t next_head = (uart->rx_head + 1) % sizeof(uart->rx_buffer);
        if (next_head != uart->rx_tail) {
            uart->rx_buffer[uart->rx_head] = data[i];
            uart->rx_head = next_head;
        }
        /* 缓冲区满则丢弃数据 */
    }
}

void stc_hal_stm32_uart_flush(stc_stm32_uart_t* uart)
{
    hal_flush(uart);
}

