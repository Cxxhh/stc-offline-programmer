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
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_gpio.h"
#include <string.h>

/* ==================== 配置定义 ==================== */

// 使用USART2作为STC通信接口
#define STC_UART                USART2

// 缓冲区大小定义
#define RX_BUFFER_SIZE          256
#define TX_BUFFER_SIZE          256

// 超时定义（毫秒）
#define DEFAULT_TIMEOUT_MS      500
#define HANDSHAKE_TIMEOUT_MS    50
#define TRANSFER_TIMEOUT_MS     15000

// DTR/RTS控制引脚定义（可选，用于自动复位）
// 如果不使用硬件控制，这些定义可以注释掉
// #define DTR_PORT                GPIOB
// #define DTR_PIN                 LL_GPIO_PIN_0
// #define RTS_PORT                GPIOB
// #define RTS_PIN                 LL_GPIO_PIN_1

/* ==================== 私有变量 ==================== */

// 接收缓冲区
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;  // 写入位置
static volatile uint16_t rx_tail = 0;  // 读取位置

// 发送缓冲区（如果需要）
static uint8_t tx_buffer[TX_BUFFER_SIZE];

// 系统tick计数器（需要在SysTick中断中递增）
static volatile uint32_t system_tick = 0;

/* ==================== 私有函数声明 ==================== */

static hal_status_enum uart_init(void);
static void uart_deinit(void);
static hal_status_enum uart_transmit(const uint8_t *data, uint16_t length);
static hal_status_enum uart_receive(uint8_t *data, uint16_t length);
static hal_status_enum uart_transmit_receive(const uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

static void delay_ms(uint32_t ms);
static void delay_us(uint32_t us);

static void gpio_write(uint8_t pin, gpio_level_enum level);
static gpio_level_enum gpio_read(uint8_t pin);

static hal_status_enum set_baudrate(uint32_t baudrate);
static uint32_t get_baudrate(void);

static uint32_t get_tick(void);
static uint16_t get_rx_count(void);
static void flush_rx(void);
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
    hal.fpreceive = uart_receive;
    hal.fptransmit_receive = uart_transmit_receive;
    
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
    hal.fpget_rx_count = get_rx_count;
    hal.fpflush_rx = flush_rx;
    hal.fpflush_tx = flush_tx;
    
    // 回调函数（默认为NULL）
    hal.tx_complete_callback = NULL;
    hal.rx_complete_callback = NULL;
    hal.error_callback = NULL;
    
    // 配置参数
    hal.timeout_ms = DEFAULT_TIMEOUT_MS;
    hal.rx_buffer_size = RX_BUFFER_SIZE;
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
 * @return HAL_OK: 成功, HAL_ERROR: 失败
 */
static hal_status_enum uart_init(void)
{
    // USART2已在MX_USART2_UART_Init中初始化
    // 这里只需清空缓冲区和重置标志
    flush_rx();
    flush_tx();
    
    return HAL_OK;
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
 * @return HAL_OK: 成功, HAL_TIMEOUT: 超时
 */
static hal_status_enum uart_transmit(const uint8_t *data, uint16_t length)
{
    uint32_t start_tick = get_tick();
    
    HAL_CHECK_NULL(data);
    
    for (uint16_t i = 0; i < length; i++)
    {
        // 等待发送寄存器为空
        while (!LL_USART_IsActiveFlag_TXE(STC_UART))
        {
            if (HAL_TIMEOUT_CHECK(start_tick, DEFAULT_TIMEOUT_MS, get_tick))
            {
                return HAL_TIMEOUT;
            }
        }
        
        // 发送数据
        LL_USART_TransmitData8(STC_UART, data[i]);
    }
    
    // 等待发送完成
    start_tick = get_tick();
    while (!LL_USART_IsActiveFlag_TC(STC_UART))
    {
        if (HAL_TIMEOUT_CHECK(start_tick, DEFAULT_TIMEOUT_MS, get_tick))
        {
            return HAL_TIMEOUT;
        }
    }
    
    return HAL_OK;
}

/**
 * @brief UART接收数据（轮询方式）
 * @param data 接收缓冲区
 * @param length 要接收的数据长度
 * @return HAL_OK: 成功, HAL_TIMEOUT: 超时
 * 
 * @note 此函数从环形缓冲区读取数据（数据由中断填充）
 *       或直接轮询UART接收（根据实际需求选择）
 */
static hal_status_enum uart_receive(uint8_t *data, uint16_t length)
{
    uint32_t start_tick = get_tick();
    uint16_t received = 0;
    
    HAL_CHECK_NULL(data);
    
    while (received < length)
    {
        // 检查是否有数据可读
        if (LL_USART_IsActiveFlag_RXNE(STC_UART))
        {
            // 读取数据
            data[received] = LL_USART_ReceiveData8(STC_UART);
            received++;
            
            // 重置超时计时器（收到数据后）
            start_tick = get_tick();
        }
        
        // 检查超时
        if (HAL_TIMEOUT_CHECK(start_tick, DEFAULT_TIMEOUT_MS, get_tick))
        {
            return HAL_TIMEOUT;
        }
    }
    
    return HAL_OK;
}

/**
 * @brief UART收发数据（先发后收）
 * @param tx_data 要发送的数据
 * @param rx_data 接收缓冲区
 * @param length 数据长度
 * @return HAL_OK: 成功, 其他: 失败
 * 
 * @note 这个函数适配STC协议的特性：发送命令后等待响应
 */
static hal_status_enum uart_transmit_receive(const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    hal_status_enum status;
    
    // 先发送
    status = uart_transmit(tx_data, length);
    if (status != HAL_OK)
    {
        return status;
    }
    
    // 再接收
    status = uart_receive(rx_data, length);
    return status;
}

/**
 * @brief STC专用接收函数：等待并接收完整的数据包
 * @param data 接收缓冲区
 * @param max_length 缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际接收的字节数，0表示超时或错误
 * 
 * @note 此函数会持续接收直到没有更多数据到达
 *       适合STC协议中等待MCU响应的场景
 */
uint16_t stc_hal_receive_packet(uint8_t *data, uint16_t max_length, uint32_t timeout_ms)
{
    uint32_t start_tick = get_tick();
    uint32_t last_data_tick = start_tick;
    uint16_t received = 0;
    const uint32_t inter_byte_timeout = 10; // 字节间超时10ms
    
    if (data == NULL || max_length == 0)
    {
        return 0;
    }
    
    while (received < max_length)
    {
        // 检查是否有数据可读
        if (LL_USART_IsActiveFlag_RXNE(STC_UART))
        {
            // 读取数据
            data[received] = LL_USART_ReceiveData8(STC_UART);
            received++;
            
            // 更新最后接收数据的时间
            last_data_tick = get_tick();
        }
        
        // 如果已经接收到数据，检查字节间超时（说明数据包接收完成）
        if (received > 0)
        {
            if (HAL_TIMEOUT_CHECK(last_data_tick, inter_byte_timeout, get_tick))
            {
                // 字节间超时，认为数据包接收完成
                break;
            }
        }
        
        // 检查总超时
        if (HAL_TIMEOUT_CHECK(start_tick, timeout_ms, get_tick))
        {
            break;
        }
    }
    
    return received;
}

/**
 * @brief 等待接收到数据的通用函数
 * @param timeout_ms 超时时间（毫秒）
 * @return true: 有数据, false: 超时无数据
 * 
 * @note 适用于握手阶段检测MCU是否有响应
 */
bool stc_hal_wait_for_data(uint32_t timeout_ms)
{
    uint32_t start_tick = get_tick();
    
    while (!LL_USART_IsActiveFlag_RXNE(STC_UART))
    {
        if (HAL_TIMEOUT_CHECK(start_tick, timeout_ms, get_tick))
        {
            return false;
        }
    }
    
    return true;
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
    if (pin == 0)  // 假设pin=0代表DTR
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
    if (pin == 1)  // 假设pin=1代表RTS
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
 * @return HAL_OK: 成功, HAL_ERROR: 失败
 * 
 * @note STC协议需要在握手后切换波特率
 */
static hal_status_enum set_baudrate(uint32_t baudrate)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};
    
    // 禁用UART
    LL_USART_Disable(STC_UART);
    
    // 等待UART禁用完成
    while (LL_USART_IsEnabled(STC_UART));
    
    // 设置新波特率
    USART_InitStruct.BaudRate = baudrate;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_9B;  // 8数据+1校验
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_EVEN;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    
    if (LL_USART_Init(STC_UART, &USART_InitStruct) != SUCCESS)
    {
        return HAL_ERROR;
    }
    
    // 重新使能UART
    LL_USART_Enable(STC_UART);
    
    // 等待UART就绪
    while ((!(LL_USART_IsActiveFlag_TEACK(STC_UART))) || 
           (!(LL_USART_IsActiveFlag_REACK(STC_UART))));
    
    // 清空缓冲区
    flush_rx();
    flush_tx();
    
    return HAL_OK;
}

/**
 * @brief 获取当前波特率
 * @return 波特率值
 */
static uint32_t get_baudrate(void)
{
    return LL_USART_GetBaudRate(STC_UART, 
                                LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE),
                                LL_USART_OVERSAMPLING_16);
}

/**
 * @brief 设置校验位
 * @param parity 校验类型（0=无校验, 1=偶校验）
 * @return HAL_OK: 成功
 * 
 * @note STC89无校验，STC12+需要偶校验
 */
hal_status_enum stc_hal_set_parity(uint8_t parity)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};
    uint32_t current_baudrate = get_baudrate();
    
    // 禁用UART
    LL_USART_Disable(STC_UART);
    while (LL_USART_IsEnabled(STC_UART));
    
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
           (!(LL_USART_IsActiveFlag_REACK(STC_UART))));
    
    return HAL_OK;
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
 * @brief 获取接收缓冲区数据量
 * @return 数据字节数
 */
static uint16_t get_rx_count(void)
{
    // 如果使用环形缓冲区
    if (rx_head >= rx_tail)
    {
        return rx_head - rx_tail;
    }
    else
    {
        return RX_BUFFER_SIZE - rx_tail + rx_head;
    }
}

/**
 * @brief 清空接收缓冲区
 */
static void flush_rx(void)
{
    rx_head = 0;
    rx_tail = 0;
    
    // 清空硬件FIFO
    while (LL_USART_IsActiveFlag_RXNE(STC_UART))
    {
        (void)LL_USART_ReceiveData8(STC_UART);
    }
}

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

/* ==================== 中断处理函数（可选） ==================== */

/**
 * @brief USART2中断处理函数（如果使用中断接收）
 * @note 需要在stm32l4xx_it.c中调用此函数
 */
void stc_hal_uart_irq_handler(void)
{
    // 接收中断
    if (LL_USART_IsActiveFlag_RXNE(STC_UART))
    {
        uint8_t data = LL_USART_ReceiveData8(STC_UART);
        
        // 写入环形缓冲区
        uint16_t next_head = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != rx_tail)
        {
            rx_buffer[rx_head] = data;
            rx_head = next_head;
        }
        // 如果缓冲区满了，数据会被丢弃
    }
    
    // 帧错误处理
    if (LL_USART_IsActiveFlag_FE(STC_UART))
    {
        LL_USART_ClearFlag_FE(STC_UART);
        // 可以在这里添加错误处理
    }
    
    // 其他错误处理...
}
