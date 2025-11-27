/**
 * @file stc_context.h
 * @brief STC烧录器运行时上下文定义
 */

#ifndef __STC_CONTEXT_H__
#define __STC_CONTEXT_H__

#include "stc_types.h"
#include "stc_protocol_config.h"
#include "stc_protocol_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MCU信息结构体
 *============================================================================*/
typedef struct {
    uint16_t    magic;              // MCU Magic值（型号识别码）
    const char* model_name;         // MCU型号名称
    uint32_t    flash_size;         // Flash大小（字节）
    uint32_t    eeprom_size;        // EEPROM大小（字节）
    float       clock_hz;           // 当前时钟频率（Hz）
    uint8_t     bsl_version;        // BSL版本号
    uint8_t     cpu_6t;             // 6T模式（1）或12T模式（0）
    uint16_t    freq_counter;       // 频率计数器值（用于频率计算）
    uint8_t     uid[STC_UID_SIZE];  // 唯一ID（7字节）
    uint8_t     uid_valid;          // UID是否有效
} stc_mcu_info_t;

/*============================================================================
 * 频率校准结果
 *============================================================================*/
typedef struct {
    uint16_t    user_trim;          // 用户频率trim值
    uint16_t    program_trim;       // 编程频率trim值
    uint8_t     trim_divider;       // trim分频器
    uint8_t     trim_range;         // trim范围
    float       final_frequency;    // 最终校准频率
} stc_trim_result_t;

/*============================================================================
 * 通信配置
 *============================================================================*/
typedef struct {
    uint32_t    baud_handshake;     // 握手波特率（默认2400）
    uint32_t    baud_transfer;      // 传输波特率（默认115200）
    uint32_t    timeout_ms;         // 默认超时时间
    uint32_t    erase_timeout_ms;   // 擦除超时时间
} stc_comm_config_t;

/*============================================================================
 * HAL接口（硬件抽象）
 *============================================================================*/
typedef struct {
    /**
     * @brief 设置波特率
     * @param handle UART句柄
     * @param baudrate 波特率
     * @return 0成功，其他失败
     */
    int (*set_baudrate)(void* handle, uint32_t baudrate);
    
    /**
     * @brief 设置校验位
     * @param handle UART句柄
     * @param parity 校验类型
     * @return 0成功，其他失败
     */
    int (*set_parity)(void* handle, stc_parity_t parity);
    
    /**
     * @brief 发送数据
     * @param handle UART句柄
     * @param data 数据指针
     * @param len 数据长度
     * @param timeout_ms 超时时间
     * @return 实际发送字节数，<0失败
     */
    int (*write)(void* handle, const uint8_t* data, uint16_t len, uint32_t timeout_ms);
    
    /**
     * @brief 接收数据
     * @param handle UART句柄
     * @param data 数据缓冲区
     * @param max_len 最大接收长度
     * @param timeout_ms 超时时间
     * @return 实际接收字节数，<0失败
     */
    int (*read)(void* handle, uint8_t* data, uint16_t max_len, uint32_t timeout_ms);
    
    /**
     * @brief 清空接收缓冲区
     * @param handle UART句柄
     */
    void (*flush)(void* handle);
    
    /**
     * @brief 延时
     * @param ms 毫秒数
     */
    void (*delay_ms)(uint32_t ms);
    
    /**
     * @brief 获取系统时间（毫秒）
     * @return 当前系统时间
     */
    uint32_t (*get_tick_ms)(void);
    
} stc_hal_t;

/*============================================================================
 * 运行时上下文
 *============================================================================*/
struct stc_context {
    /* 协议配置和操作 */
    const stc_protocol_config_t*    config;     // 协议配置
    const stc_protocol_ops_t*       ops;        // 协议操作
    
    /* 协议选择 */
    stc_select_mode_t       select_mode;        // 选择模式
    stc_protocol_id_t       manual_proto_id;    // 手动选择的协议ID
    stc_protocol_id_t       detected_proto_id;  // 检测到的协议ID
    uint8_t                 proto_detected;     // 是否已检测到协议
    
    /* MCU信息 */
    stc_mcu_info_t          mcu_info;           // MCU信息
    
    /* 频率校准结果 */
    stc_trim_result_t       trim_result;        // 校准结果
    
    /* 通信配置 */
    stc_comm_config_t       comm_config;        // 通信配置
    
    /* HAL接口 */
    const stc_hal_t*        hal;                // HAL接口
    void*                   uart_handle;        // UART句柄
    
    /* 回调函数 */
    stc_progress_cb_t       progress_cb;        // 进度回调
    void*                   progress_user_data; // 进度回调用户数据
    stc_log_cb_t            log_cb;             // 日志回调
    void*                   log_user_data;      // 日志回调用户数据
    
    /* 内部缓冲区 */
    uint8_t                 tx_buffer[STC_MAX_PACKET_SIZE];
    uint8_t                 rx_buffer[STC_MAX_PACKET_SIZE];
    uint16_t                rx_len;             // 接收数据长度
    
    /* 状态包原始数据（用于协议解析） */
    uint8_t                 status_packet[STC_MAX_PAYLOAD_SIZE];
    uint16_t                status_packet_len;
};

/*============================================================================
 * 上下文管理函数
 *============================================================================*/

/**
 * @brief 初始化上下文
 * @param ctx 上下文指针
 * @param hal HAL接口
 * @param uart_handle UART句柄
 */
void stc_context_init(stc_context_t* ctx, const stc_hal_t* hal, void* uart_handle);

/**
 * @brief 重置上下文（保留HAL配置）
 * @param ctx 上下文指针
 */
void stc_context_reset(stc_context_t* ctx);

/**
 * @brief 设置通信参数
 * @param ctx 上下文指针
 * @param baud_handshake 握手波特率
 * @param baud_transfer 传输波特率
 */
void stc_context_set_baudrate(stc_context_t* ctx, uint32_t baud_handshake, uint32_t baud_transfer);

/**
 * @brief 设置进度回调
 * @param ctx 上下文指针
 * @param cb 回调函数
 * @param user_data 用户数据
 */
void stc_context_set_progress_callback(stc_context_t* ctx, stc_progress_cb_t cb, void* user_data);

/**
 * @brief 设置日志回调
 * @param ctx 上下文指针
 * @param cb 回调函数
 * @param user_data 用户数据
 */
void stc_context_set_log_callback(stc_context_t* ctx, stc_log_cb_t cb, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* __STC_CONTEXT_H__ */

