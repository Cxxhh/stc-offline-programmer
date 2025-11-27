/**
 * @file stc8_protocol.h
 * @brief STC8系列协议实现（STC8/STC8d/STC8g）
 */

#ifndef __STC8_PROTOCOL_H__
#define __STC8_PROTOCOL_H__

#include "../stc_types.h"
#include "../stc_context.h"
#include "stc15_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STC8协议常量
 *============================================================================*/
#define STC8_PROGRAM_FREQ       24000000.0f     // STC8使用24MHz编程频率

/*============================================================================
 * STC8协议操作声明
 *============================================================================*/
extern const stc_protocol_ops_t stc8_protocol_ops;
extern const stc_protocol_ops_t stc8d_protocol_ops;
extern const stc_protocol_ops_t stc8g_protocol_ops;
extern const stc_protocol_ops_t stc32_protocol_ops;

/*============================================================================
 * STC8协议函数
 *============================================================================*/

/**
 * @brief STC8频率校准（分频器概念）
 */
int stc8_calibrate_frequency(stc_context_t* ctx, float target_freq);

/**
 * @brief STC8d频率校准（特殊校准包格式）
 */
int stc8d_calibrate_frequency(stc_context_t* ctx, float target_freq);

/**
 * @brief STC8g频率校准（需要epilogue）
 */
int stc8g_calibrate_frequency(stc_context_t* ctx, float target_freq);

/**
 * @brief STC8设置选项字节（40字节格式）
 */
int stc8_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __STC8_PROTOCOL_H__ */

