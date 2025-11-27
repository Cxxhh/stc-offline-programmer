/**
 * @file stc12_protocol.h
 * @brief STC12系列协议实现
 */

#ifndef __STC12_PROTOCOL_H__
#define __STC12_PROTOCOL_H__

#include "../stc_types.h"
#include "../stc_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STC12协议常量
 *============================================================================*/
#define STC12_ERASE_COUNTDOWN_END   0x0D    // 擦除倒计时结束值

/*============================================================================
 * STC12协议操作声明
 *============================================================================*/
extern const stc_protocol_ops_t stc12_protocol_ops;

/*============================================================================
 * STC12协议函数
 *============================================================================*/

/**
 * @brief 解析STC12状态包
 */
int stc12_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len);

/**
 * @brief STC12握手流程
 */
int stc12_handshake(stc_context_t* ctx);

/**
 * @brief STC12擦除Flash（带倒计时）
 */
int stc12_erase_flash(stc_context_t* ctx, uint32_t size);

/**
 * @brief STC12编程单个数据块
 */
int stc12_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);

/**
 * @brief STC12编程完成
 */
int stc12_program_finish(stc_context_t* ctx);

/**
 * @brief STC12设置选项字节
 */
int stc12_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

/**
 * @brief STC12断开连接
 */
int stc12_disconnect(stc_context_t* ctx);

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * @brief 计算STC12的BRT寄存器值（8位）
 * @param mcu_clock_hz MCU时钟频率
 * @param baud_transfer 目标波特率
 * @return BRT值
 */
uint8_t stc12_calc_brt(float mcu_clock_hz, uint32_t baud_transfer);

/**
 * @brief 获取STC12的IAP等待状态
 * @param clock_hz 时钟频率
 * @return IAP等待状态值
 */
uint8_t stc12_get_iap_delay(float clock_hz);

#ifdef __cplusplus
}
#endif

#endif /* __STC12_PROTOCOL_H__ */

