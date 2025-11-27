/**
 * @file stc_isp.h
 * @brief STC ISP烧录器主头文件
 * 
 * 包含所有必要的头文件，提供统一的接口访问
 */

#ifndef __STC_ISP_H__
#define __STC_ISP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 核心模块 */
#include "stc_types.h"
#include "stc_protocol_config.h"
#include "stc_protocol_ops.h"
#include "stc_context.h"
#include "stc_packet.h"
#include "stc_model_db.h"
#include "stc_programmer.h"

/* 协议实现 */
#include "protocols/stc89_protocol.h"
#include "protocols/stc12_protocol.h"
#include "protocols/stc15_protocol.h"
#include "protocols/stc8_protocol.h"

/* HAL层 - 根据平台选择 */
#ifdef STM32_PLATFORM
#include "hal/stc_hal_stm32.h"
#endif

/*============================================================================
 * 快速使用示例
 *============================================================================
 * 
 * // 1. 初始化
 * stc_stm32_uart_t uart;
 * stc_hal_stm32_uart_init(&uart, &huart1);
 * 
 * stc_context_t ctx;
 * stc_programmer_init(&ctx, stc_hal_stm32_get(), &uart);
 * 
 * // 2. 设置模式
 * stc_set_mode_auto(&ctx);  // 或 stc_set_mode_manual(&ctx, STC_PROTO_STC8D);
 * 
 * // 3. 连接MCU（等待上电）
 * int ret = stc_connect(&ctx, 30000);  // 30秒超时
 * if (ret == STC_ERR_UNKNOWN_MODEL) {
 *     // 未知型号，让用户手动选择
 *     stc_set_mode_manual(&ctx, user_selected_protocol);
 * }
 * 
 * // 4. 选择协议
 * stc_select_protocol(&ctx);
 * 
 * // 5. 获取MCU信息
 * const stc_mcu_info_t* info = stc_get_mcu_info(&ctx);
 * printf("MCU: %s, Flash: %lu KB\n", info->model_name, info->flash_size / 1024);
 * 
 * // 6. 烧录
 * ret = stc_program(&ctx, firmware_data, firmware_len, NULL);
 * if (ret != STC_OK) {
 *     printf("烧录失败: %s\n", stc_get_error_string(ret));
 * }
 * 
 * // 7. 断开
 * stc_disconnect(&ctx);
 * 
 *============================================================================*/

/*============================================================================
 * 版本信息
 *============================================================================*/
#define STC_ISP_VERSION_MAJOR   1
#define STC_ISP_VERSION_MINOR   0
#define STC_ISP_VERSION_PATCH   0
#define STC_ISP_VERSION_STRING  "1.0.0"

/**
 * @brief 获取库版本字符串
 * @return 版本字符串
 */
static inline const char* stc_isp_get_version(void) {
    return STC_ISP_VERSION_STRING;
}

#ifdef __cplusplus
}
#endif

#endif /* __STC_ISP_H__ */

