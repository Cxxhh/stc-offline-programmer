/**
 * @file stc15_protocol.h
 * @brief STC15系列协议实现（含频率校准）
 */

#ifndef __STC15_PROTOCOL_H__
#define __STC15_PROTOCOL_H__

#include "../stc_types.h"
#include "../stc_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STC15协议常量
 *============================================================================*/
#define STC15_PROGRAM_FREQ      22118400.0f     // 固定编程频率 22.1184 MHz
#define STC15_PROGRAM_FREQ_24M  24000000.0f     // STC8使用24MHz编程频率

/*============================================================================
 * STC15协议操作声明
 *============================================================================*/
extern const stc_protocol_ops_t stc15_protocol_ops;
extern const stc_protocol_ops_t stc15a_protocol_ops;

/*============================================================================
 * STC15协议内部函数（供STC8等继承使用）
 *============================================================================*/

/**
 * @brief 解析STC15状态包
 */
int stc15_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len);

/**
 * @brief STC15握手流程
 */
int stc15_handshake(stc_context_t* ctx);

/**
 * @brief STC15频率校准
 */
int stc15_calibrate_frequency(stc_context_t* ctx, float target_freq);

/**
 * @brief STC15A频率校准（两轮校准）
 */
int stc15a_calibrate_frequency(stc_context_t* ctx, float target_freq);

/**
 * @brief STC15擦除Flash
 */
int stc15_erase_flash(stc_context_t* ctx, uint32_t size);

/**
 * @brief STC15编程单个数据块
 */
int stc15_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);

/**
 * @brief STC15编程完成
 */
int stc15_program_finish(stc_context_t* ctx);

/**
 * @brief STC15设置选项字节
 */
int stc15_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

/**
 * @brief STC15断开连接
 */
int stc15_disconnect(stc_context_t* ctx);

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * @brief 获取IAP等待状态值
 * @param clock_hz 时钟频率
 * @return IAP等待状态值
 */
uint8_t stc15_get_iap_delay(float clock_hz);

/**
 * @brief 计算BRT寄存器值
 * @param program_freq 编程频率
 * @param baud_transfer 传输波特率
 * @return BRT值
 */
uint16_t stc15_calc_brt(float program_freq, uint32_t baud_transfer);

/**
 * @brief 发送同步脉冲
 * @param ctx 上下文
 * @param count 脉冲数
 * @param interval_ms 间隔毫秒
 * @return STC_OK成功
 */
int stc15_pulse_sync(stc_context_t* ctx, uint16_t count, uint16_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* __STC15_PROTOCOL_H__ */

