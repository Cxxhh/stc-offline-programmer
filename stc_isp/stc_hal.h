/**
 * @file stc_hal.h
 * @brief STC ISP Programming Hardware Abstraction Layer
 * @version 1.1
 * @date 2025-11-30
 */

#ifndef STC_HAL_H
#define STC_HAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief HAL状态枚举
     */
    typedef enum
    {
        HAL_OK = 0,      /**< 操作成功 */
        HAL_ERROR,       /**< 操作失败 */
        HAL_BUSY,        /**< HAL忙碌中 */
        HAL_TIMEOUT      /**< 操作超时 */
    } hal_status_enum;

    /**
     * @brief GPIO电平枚举
     */
    typedef enum
    {
        GPIO_LOW = 0,    /**< 低电平 */
        GPIO_HIGH        /**< 高电平 */
    } gpio_level_enum;

    /**
     * @brief STC HAL回调函数类型定义
     */
    typedef void (*hal_callback_t)(void);
    typedef void (*hal_error_callback_t)(hal_status_enum error);

    /**
     * @brief STC硬件抽象层结构体
     */
    typedef struct
    {
        /* 基本通信接口 */
        hal_status_enum (*fpinit)(void);                                      /**< 初始化函数 */
        void (*fpdeinit)(void);                                              /**< 反初始化函数 */
        hal_status_enum (*fptransmit)(const uint8_t *data, uint16_t length); /**< 发送数据 */
        hal_status_enum (*fpreceive)(uint8_t *data, uint16_t length);        /**< 接收数据 */
        hal_status_enum (*fptransmit_receive)(const uint8_t *tx_data, uint8_t *rx_data, uint16_t length); /**< 收发数据 */
        
        /* 延时函数 - STC协议需要精确延时 */
        void (*fpdelay_ms)(uint32_t ms);                                     /**< 毫秒延时 */
        void (*fpdelay_us)(uint32_t us);                                     /**< 微秒延时 */
        
        /* GPIO控制 - 用于硬件复位/控制引脚 */
        void (*fpgpio_write)(uint8_t pin, gpio_level_enum level);           /**< GPIO写 */
        gpio_level_enum (*fpgpio_read)(uint8_t pin);                        /**< GPIO读 */
        
        /* 波特率配置 - 串口通信必需 */
        hal_status_enum (*fpset_baudrate)(uint32_t baudrate);               /**< 设置波特率 */
        uint32_t (*fpget_baudrate)(void);                                   /**< 获取波特率 */
        
        /* 超时控制 */
        uint32_t (*fpget_tick)(void);                                       /**< 获取系统tick(ms) */
        
        /* 数据可用性检查 */
        uint16_t (*fpget_rx_count)(void);                                   /**< 获取接收缓冲区数据量 */
        void (*fpflush_rx)(void);                                           /**< 清空接收缓冲区 */
        void (*fpflush_tx)(void);                                           /**< 清空发送缓冲区 */
        
        /* 回调函数 */
        hal_callback_t tx_complete_callback;                                /**< 发送完成回调 */
        hal_callback_t rx_complete_callback;                                /**< 接收完成回调 */
        hal_error_callback_t error_callback;                                /**< 错误回调 */
        
        /* 配置参数 */
        uint32_t timeout_ms;                                                /**< 默认超时时间(ms) */
        uint16_t rx_buffer_size;                                            /**< 接收缓冲区大小 */
        uint16_t tx_buffer_size;                                            /**< 发送缓冲区大小 */
        
    } stc_hal_t;

    /**
     * @brief HAL辅助宏定义
     */
    #define HAL_CHECK_NULL(ptr) \
        do { \
            if ((ptr) == NULL) { \
                return HAL_ERROR; \
            } \
        } while(0)

    #define HAL_TIMEOUT_CHECK(start_tick, timeout_ms, get_tick_func) \
        ((get_tick_func() - (start_tick)) > (timeout_ms))

#ifdef __cplusplus
}
#endif

#endif /* STC_HAL_H */
