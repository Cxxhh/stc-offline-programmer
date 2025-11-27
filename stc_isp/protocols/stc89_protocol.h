/**
 * @file stc89_protocol.h
 * @brief STC89/89A系列协议实现
 */

#ifndef __STC89_PROTOCOL_H__
#define __STC89_PROTOCOL_H__

#include "../stc_types.h"
#include "../stc_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STC89协议操作声明
 *============================================================================*/
extern const stc_protocol_ops_t stc89_protocol_ops;
extern const stc_protocol_ops_t stc89a_protocol_ops;

/*============================================================================
 * STC89协议函数
 *============================================================================*/

/**
 * @brief 解析STC89状态包
 */
int stc89_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len);

/**
 * @brief STC89握手流程
 */
int stc89_handshake(stc_context_t* ctx);

/**
 * @brief STC89擦除Flash
 */
int stc89_erase_flash(stc_context_t* ctx, uint32_t size);

/**
 * @brief STC89编程单个数据块
 */
int stc89_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);

/**
 * @brief STC89设置选项字节
 */
int stc89_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

/**
 * @brief STC89断开连接
 */
int stc89_disconnect(stc_context_t* ctx);

/*============================================================================
 * STC89A协议函数
 *============================================================================*/

/**
 * @brief 解析STC89A状态包
 */
int stc89a_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len);

/**
 * @brief STC89A握手流程
 */
int stc89a_handshake(stc_context_t* ctx);

/**
 * @brief STC89A擦除Flash（全擦除）
 */
int stc89a_erase_flash(stc_context_t* ctx, uint32_t size);

/**
 * @brief STC89A编程单个数据块
 */
int stc89a_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);

/**
 * @brief STC89A设置选项字节
 */
int stc89a_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

/**
 * @brief STC89A断开连接
 */
int stc89a_disconnect(stc_context_t* ctx);

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * @brief 计算STC89的BRT寄存器值（16位）
 * @param mcu_clock_hz MCU时钟频率
 * @param baud_transfer 目标波特率
 * @param cpu_6t 是否为6T模式
 * @return BRT值
 */
uint16_t stc89_calc_brt(float mcu_clock_hz, uint32_t baud_transfer, uint8_t cpu_6t);

/**
 * @brief 获取STC89的IAP等待状态
 * @param clock_hz 时钟频率
 * @return IAP等待状态值
 */
uint8_t stc89_get_iap_delay(float clock_hz);

#ifdef __cplusplus
}
#endif

#endif /* __STC89_PROTOCOL_H__ */

