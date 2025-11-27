/**
 * @file stc_protocol_ops.h
 * @brief STC协议操作接口定义（函数指针）
 */

#ifndef __STC_PROTOCOL_OPS_H__
#define __STC_PROTOCOL_OPS_H__

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct stc_context stc_context_t;

/*============================================================================
 * 协议操作接口（虚函数表）
 *============================================================================*/
typedef struct {
    /**
     * @brief 解析状态包
     * @param ctx 上下文
     * @param data 状态包数据
     * @param len 数据长度
     * @return STC_OK成功，其他失败
     */
    int (*parse_status_packet)(stc_context_t* ctx, const uint8_t* data, uint16_t len);
    
    /**
     * @brief 握手与波特率切换
     * @param ctx 上下文
     * @return STC_OK成功，其他失败
     */
    int (*handshake)(stc_context_t* ctx);
    
    /**
     * @brief 频率校准（STC15+需要，STC89/12返回STC_OK跳过）
     * @param ctx 上下文
     * @param target_freq 目标频率（0表示使用当前频率）
     * @return STC_OK成功，其他失败
     */
    int (*calibrate_frequency)(stc_context_t* ctx, float target_freq);
    
    /**
     * @brief 擦除Flash
     * @param ctx 上下文
     * @param size 需要擦除的大小
     * @return STC_OK成功，其他失败
     */
    int (*erase_flash)(stc_context_t* ctx, uint32_t size);
    
    /**
     * @brief 编程Flash单个数据块
     * @param ctx 上下文
     * @param addr 写入地址
     * @param data 数据指针
     * @param len 数据长度
     * @param is_first 是否为首块
     * @return STC_OK成功，其他失败
     */
    int (*program_block)(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);
    
    /**
     * @brief 编程完成确认
     * @param ctx 上下文
     * @return STC_OK成功，其他失败
     */
    int (*program_finish)(stc_context_t* ctx);
    
    /**
     * @brief 设置选项字节
     * @param ctx 上下文
     * @param options 选项字节数据
     * @param len 选项字节长度
     * @return STC_OK成功，其他失败
     */
    int (*set_options)(stc_context_t* ctx, const uint8_t* options, uint8_t len);
    
    /**
     * @brief 断开连接
     * @param ctx 上下文
     * @return STC_OK成功，其他失败
     */
    int (*disconnect)(stc_context_t* ctx);
    
} stc_protocol_ops_t;

/*============================================================================
 * 协议操作声明（各协议实现）
 *============================================================================*/
extern const stc_protocol_ops_t stc89_protocol_ops;
extern const stc_protocol_ops_t stc89a_protocol_ops;
extern const stc_protocol_ops_t stc12_protocol_ops;
extern const stc_protocol_ops_t stc15a_protocol_ops;
extern const stc_protocol_ops_t stc15_protocol_ops;
extern const stc_protocol_ops_t stc8_protocol_ops;
extern const stc_protocol_ops_t stc8d_protocol_ops;
extern const stc_protocol_ops_t stc8g_protocol_ops;
extern const stc_protocol_ops_t stc32_protocol_ops;
extern const stc_protocol_ops_t usb15_protocol_ops;

#ifdef __cplusplus
}
#endif

#endif /* __STC_PROTOCOL_OPS_H__ */

