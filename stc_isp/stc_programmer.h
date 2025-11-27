/**
 * @file stc_programmer.h
 * @brief STC烧录器主控流程
 */

#ifndef __STC_PROGRAMMER_H__
#define __STC_PROGRAMMER_H__

#include "stc_types.h"
#include "stc_context.h"
#include "stc_model_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 烧录器配置
 *============================================================================*/
typedef struct {
    uint32_t    baud_handshake;     // 握手波特率（0使用默认2400）
    uint32_t    baud_transfer;      // 传输波特率（0使用默认115200）
    float       target_frequency;   // 目标频率（0使用当前频率）
    uint8_t     erase_eeprom;       // 是否同时擦除EEPROM
    uint8_t     verify_after_write; // 写入后是否验证
} stc_program_config_t;

/*============================================================================
 * 主要API
 *============================================================================*/

/**
 * @brief 初始化烧录器
 * @param ctx 上下文指针
 * @param hal HAL接口
 * @param uart_handle UART句柄
 * @return STC_OK成功
 */
int stc_programmer_init(stc_context_t* ctx, const stc_hal_t* hal, void* uart_handle);

/**
 * @brief 设置为自动识别模式
 * @param ctx 上下文指针
 * @return STC_OK成功
 */
int stc_set_mode_auto(stc_context_t* ctx);

/**
 * @brief 设置为手动选择模式
 * @param ctx 上下文指针
 * @param proto_id 协议ID
 * @return STC_OK成功
 */
int stc_set_mode_manual(stc_context_t* ctx, stc_protocol_id_t proto_id);

/**
 * @brief 连接并识别MCU
 * @param ctx 上下文指针
 * @param timeout_ms 超时时间（毫秒），0表示一直等待
 * @return STC_OK成功，STC_ERR_UNKNOWN_MODEL表示未知型号
 */
int stc_connect(stc_context_t* ctx, uint32_t timeout_ms);

/**
 * @brief 选择协议（连接成功后调用）
 * @param ctx 上下文指针
 * @return STC_OK成功
 */
int stc_select_protocol(stc_context_t* ctx);

/**
 * @brief 执行完整烧录流程
 * @param ctx 上下文指针
 * @param data 固件数据
 * @param len 数据长度
 * @param config 烧录配置（NULL使用默认）
 * @return STC_OK成功
 */
int stc_program(stc_context_t* ctx, const uint8_t* data, uint32_t len, 
                const stc_program_config_t* config);

/**
 * @brief 仅执行擦除
 * @param ctx 上下文指针
 * @param erase_eeprom 是否擦除EEPROM
 * @return STC_OK成功
 */
int stc_erase_only(stc_context_t* ctx, uint8_t erase_eeprom);

/**
 * @brief 断开连接
 * @param ctx 上下文指针
 * @return STC_OK成功
 */
int stc_disconnect(stc_context_t* ctx);

/**
 * @brief 获取MCU信息
 * @param ctx 上下文指针
 * @return MCU信息指针
 */
const stc_mcu_info_t* stc_get_mcu_info(stc_context_t* ctx);

/**
 * @brief 获取检测到的协议ID
 * @param ctx 上下文指针
 * @return 协议ID，STC_PROTO_COUNT表示未检测到
 */
stc_protocol_id_t stc_get_detected_protocol(stc_context_t* ctx);

/**
 * @brief 获取错误信息字符串
 * @param error 错误码
 * @return 错误描述字符串
 */
const char* stc_get_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* __STC_PROGRAMMER_H__ */

