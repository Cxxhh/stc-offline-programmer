/**
 * @file stc_hal.c
 * @brief STC ISP Programming Hardware Abstraction Layer Implementation
 * @version 1.0
 * @date 2025-11-30
 *
 * @note 本实现基于STM32 LL库，使用USART2作为STC ISP通信接口
 *       采用轮询方式实现，适配STC烧录协议的一发一收特性
 */

#include "stc_hal.h"
#include "usart.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"
#include <string.h>

/* ==================== 配置定义 ==================== */

// 使用USART2作为STC通信接口
#define STC_UART USART2

// 缓冲区大小定义
#define TX_BUFFER_SIZE 256

// 超时定义（毫秒）
#define DEFAULT_TIMEOUT_MS 500
#define HANDSHAKE_TIMEOUT_MS 50
#define TRANSFER_TIMEOUT_MS 15000

// DTR/RTS控制引脚定义（可选，用于自动复位）
// 如果不使用硬件控制，这些定义可以注释掉
// #define DTR_PORT                GPIOB
// #define DTR_PIN                 LL_GPIO_PIN_0
// #define RTS_PORT                GPIOB
// #define RTS_PIN                 LL_GPIO_PIN_1

/* ==================== 私有变量 ==================== */

// 发送缓冲区（如果需要）
static uint8_t tx_buffer[TX_BUFFER_SIZE];

// 系统tick计数器（需要在SysTick中断中递增）
static volatile uint32_t system_tick = 0;

/* ==================== 私有函数声明 ==================== */

static stc_hal_status_enum uart_init(void);
static void uart_deinit(void);
static stc_hal_status_enum uart_transmit(const uint8_t *data, uint16_t length);

static void delay_ms(uint32_t ms);
static void delay_us(uint32_t us);

static void gpio_write(uint8_t pin, gpio_level_enum level);
static gpio_level_enum gpio_read(uint8_t pin);

static stc_hal_status_enum set_baudrate(uint32_t baudrate);
static uint32_t get_baudrate(void);

static uint32_t get_tick(void);
static void flush_tx(void);

/* ==================== 公共函数 ==================== */

/**
 * @brief 获取STC HAL实例（带默认配置）
 * @return stc_hal_t 结构体实例
 */
stc_hal_t stc_hal_get_instance(void)
{
    stc_hal_t hal;

    // 基本通信接口
    hal.fpinit = uart_init;
    hal.fpdeinit = uart_deinit;
    hal.fptransmit = uart_transmit;

    // 延时函数
    hal.fpdelay_ms = delay_ms;
    hal.fpdelay_us = delay_us;

    // GPIO控制
    hal.fpgpio_write = gpio_write;
    hal.fpgpio_read = gpio_read;

    // 波特率配置
    hal.fpset_baudrate = set_baudrate;
    hal.fpget_baudrate = get_baudrate;

    // 超时控制
    hal.fpget_tick = get_tick;

    // 缓冲区管理
    hal.fpflush_tx = flush_tx;

    // 回调函数（默认为NULL）
    hal.tx_complete_callback = NULL;
    hal.rx_complete_callback = NULL;
    hal.error_callback = NULL;

    // 配置参数
    hal.timeout_ms = DEFAULT_TIMEOUT_MS;
    hal.tx_buffer_size = TX_BUFFER_SIZE;

    return hal;
}

/**
 * @brief 系统Tick递增（需要在SysTick_Handler中调用）
 */
void stc_hal_tick_inc(void)
{
    system_tick++;
}

/* ==================== UART相关函数 ==================== */

/**
 * @brief UART初始化
 * @return STC_HAL_OK: 成功, STC_HAL_ERROR: 失败
 */
static stc_hal_status_enum uart_init(void)
{
    // USART2已在MX_USART2_UART_Init中初始化
    // 这里只需清空缓冲区和重置标志
    flush_tx();

    return STC_HAL_OK;
}

/**
 * @brief UART反初始化
 */
static void uart_deinit(void)
{
    // 如果需要完全关闭UART，可以在这里添加代码
    // LL_USART_Disable(STC_UART);
}

/**
 * @brief UART发送数据（轮询方式）
 * @param data 要发送的数据
 * @param length 数据长度
 * @return STC_HAL_OK: 成功, STC_HAL_TIMEOUT: 超时
 */
static stc_hal_status_enum uart_transmit(const uint8_t *data, uint16_t length)
{
    uint32_t start_tick = get_tick();

    STC_HAL_CHECK_NULL(data);

    for (uint16_t i = 0; i < length; i++)
    {
        // 等待发送寄存器为空
        while (!LL_USART_IsActiveFlag_TXE(STC_UART))
        {
            if (STC_HAL_TIMEOUT_CHECK(start_tick, DEFAULT_TIMEOUT_MS, get_tick))
            {
                return STC_HAL_TIMEOUT;
            }
        }

        // 发送数据
        LL_USART_TransmitData8(STC_UART, data[i]);
    }

    // 等待发送完成
    start_tick = get_tick();
    while (!LL_USART_IsActiveFlag_TC(STC_UART))
    {
        if (STC_HAL_TIMEOUT_CHECK(start_tick, DEFAULT_TIMEOUT_MS, get_tick))
        {
            return STC_HAL_TIMEOUT;
        }
    }

    return STC_HAL_OK;
}

/* ==================== 延时函数 ==================== */

/**
 * @brief 毫秒延时
 * @param ms 延时时间（毫秒）
 */
static void delay_ms(uint32_t ms)
{
    uint32_t start = get_tick();
    while ((get_tick() - start) < ms)
    {
        // 等待
    }
}

/**
 * @brief 微秒延时（粗略实现）
 * @param us 延时时间（微秒）
 */
static void delay_us(uint32_t us)
{
    // 基于系统时钟的粗略延时
    // 假设系统时钟80MHz，每个循环约10指令周期
    uint32_t count = us * (SystemCoreClock / 10000000);
    while (count--)
    {
        __NOP();
    }
}

/* ==================== GPIO控制函数 ==================== */

/**
 * @brief GPIO写
 * @param pin 引脚编号（用户自定义）
 * @param level 电平
 */
static void gpio_write(uint8_t pin, gpio_level_enum level)
{
#ifdef DTR_PORT
    // 示例：控制DTR引脚
    if (pin == 0) // 假设pin=0代表DTR
    {
        if (level == GPIO_HIGH)
        {
            LL_GPIO_SetOutputPin(DTR_PORT, DTR_PIN);
        }
        else
        {
            LL_GPIO_ResetOutputPin(DTR_PORT, DTR_PIN);
        }
    }
#endif

#ifdef RTS_PORT
    // 示例：控制RTS引脚
    if (pin == 1) // 假设pin=1代表RTS
    {
        if (level == GPIO_HIGH)
        {
            LL_GPIO_SetOutputPin(RTS_PORT, RTS_PIN);
        }
        else
        {
            LL_GPIO_ResetOutputPin(RTS_PORT, RTS_PIN);
        }
    }
#endif

    (void)pin;
    (void)level;
}

/**
 * @brief GPIO读
 * @param pin 引脚编号
 * @return GPIO电平
 */
static gpio_level_enum gpio_read(uint8_t pin)
{
#ifdef DTR_PORT
    if (pin == 0)
    {
        return LL_GPIO_IsInputPinSet(DTR_PORT, DTR_PIN) ? GPIO_HIGH : GPIO_LOW;
    }
#endif

    (void)pin;
    return GPIO_LOW;
}

/* ==================== 波特率控制函数 ==================== */

/**
 * @brief 设置波特率
 * @param baudrate 波特率
 * @return STC_HAL_OK: 成功, STC_HAL_ERROR: 失败
 *
 * @note STC协议需要在握手后切换波特率
 */
static stc_hal_status_enum set_baudrate(uint32_t baudrate)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};

    // 禁用UART
    LL_USART_Disable(STC_UART);

    // 等待UART禁用完成
    while (LL_USART_IsEnabled(STC_UART))
        ;

    // 设置新波特率
    USART_InitStruct.BaudRate = baudrate;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_9B; // 8数据+1校验
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_EVEN;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

    if (LL_USART_Init(STC_UART, &USART_InitStruct) != SUCCESS)
    {
        return STC_HAL_ERROR;
    }

    // 重新使能UART
    LL_USART_Enable(STC_UART);

    // 等待UART就绪
    while ((!(LL_USART_IsActiveFlag_TEACK(STC_UART))) ||
           (!(LL_USART_IsActiveFlag_REACK(STC_UART))))
        ;

    // 清空缓冲区
    flush_tx();

    return STC_HAL_OK;
}

/**
 * @brief 获取当前波特率
 * @return 波特率值
 */
static uint32_t get_baudrate(void)
{
    return LL_USART_GetBaudRate(STC_UART,
                                LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE),
                                LL_USART_PRESCALER_DIV1,
                                LL_USART_OVERSAMPLING_16);
}

/**
 * @brief 设置校验位
 * @param parity 校验类型（0=无校验, 1=偶校验）
 * @return STC_HAL_OK: 成功
 *
 * @note STC89无校验，STC12+需要偶校验
 */
stc_hal_status_enum stc_hal_set_parity(uint8_t parity)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};
    uint32_t current_baudrate = get_baudrate();

    // 禁用UART
    LL_USART_Disable(STC_UART);
    while (LL_USART_IsEnabled(STC_UART))
        ;

    // 配置校验位
    USART_InitStruct.BaudRate = current_baudrate;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

    if (parity == 0)
    {
        // 无校验
        USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
        USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    }
    else
    {
        // 偶校验
        USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_9B;
        USART_InitStruct.Parity = LL_USART_PARITY_EVEN;
    }

    LL_USART_Init(STC_UART, &USART_InitStruct);

    // 重新使能
    LL_USART_Enable(STC_UART);
    while ((!(LL_USART_IsActiveFlag_TEACK(STC_UART))) ||
           (!(LL_USART_IsActiveFlag_REACK(STC_UART))))
        ;

    return STC_HAL_OK;
}

/* ==================== 时间管理函数 ==================== */

/**
 * @brief 获取系统tick（毫秒）
 * @return 系统tick值
 */
static uint32_t get_tick(void)
{
    return system_tick;
}

/* ==================== 缓冲区管理函数 ==================== */

/**
 * @brief 清空发送缓冲区
 */
static void flush_tx(void)
{
    // 等待发送完成
    while (!LL_USART_IsActiveFlag_TC(STC_UART))
    {
        // 等待
    }
}
