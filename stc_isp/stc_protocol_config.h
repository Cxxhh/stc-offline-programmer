/**
 * @file stc_protocol_config.h
 * @brief STC协议配置表定义（静态参数）
 */

#ifndef __STC_PROTOCOL_CONFIG_H__
#define __STC_PROTOCOL_CONFIG_H__

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 协议配置结构体
 *============================================================================*/
typedef struct {
    const char*         name;               // 协议名称 "STC89", "STC15A"等
    stc_checksum_type_t checksum_type;      // 校验和类型
    stc_parity_t        parity;             // 串口校验位
    stc_brt_width_t     brt_width;          // BRT寄存器宽度
    uint16_t            block_size;         // 编程块大小 (64或128)
    uint8_t             needs_freq_calib;   // 是否需要频率校准
    uint8_t             option_bytes_len;   // 选项字节长度
    uint8_t             erase_countdown;    // 擦除倒计时值（0表示无）
    uint8_t             has_uid;            // 是否支持UID读取
    uint8_t             parity_switch;      // 是否需要握手后切换校验位
    uint8_t             bsl_magic_72;       // BSL 7.2+需要5A A5魔术字
} stc_protocol_config_t;

/*============================================================================
 * 预定义协议配置
 *============================================================================*/

// STC89系列配置
static const stc_protocol_config_t stc_config_stc89 = {
    .name               = "STC89",
    .checksum_type      = STC_CHECKSUM_SINGLE_BYTE,
    .parity             = STC_PARITY_NONE,
    .brt_width          = STC_BRT_WIDTH_16BIT,
    .block_size         = STC_BLOCK_SIZE_128,
    .needs_freq_calib   = 0,
    .option_bytes_len   = 1,
    .erase_countdown    = 0,
    .has_uid            = 0,
    .parity_switch      = 0,
    .bsl_magic_72       = 0,
};

// STC89A系列配置
static const stc_protocol_config_t stc_config_stc89a = {
    .name               = "STC89A",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_16BIT,
    .block_size         = STC_BLOCK_SIZE_128,
    .needs_freq_calib   = 0,
    .option_bytes_len   = 4,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 1,    // 握手后切换为偶校验
    .bsl_magic_72       = 0,
};

// STC12系列配置
static const stc_protocol_config_t stc_config_stc12 = {
    .name               = "STC12",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_8BIT,
    .block_size         = STC_BLOCK_SIZE_128,
    .needs_freq_calib   = 0,
    .option_bytes_len   = 4,
    .erase_countdown    = 0x0D,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 0,
};

// STC15A系列配置
static const stc_protocol_config_t stc_config_stc15a = {
    .name               = "STC15A",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 13,
    .erase_countdown    = 0x5E,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 0,
};

// STC15系列配置
static const stc_protocol_config_t stc_config_stc15 = {
    .name               = "STC15",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 1,    // BSL 7.2+
};

// STC8系列配置
static const stc_protocol_config_t stc_config_stc8 = {
    .name               = "STC8",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 1,
};

// STC8D系列配置 (STC8H)
static const stc_protocol_config_t stc_config_stc8d = {
    .name               = "STC8D",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 1,
};

// STC8G系列配置 (STC8H1K)
static const stc_protocol_config_t stc_config_stc8g = {
    .name               = "STC8G",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 1,
};

// STC32系列配置
static const stc_protocol_config_t stc_config_stc32 = {
    .name               = "STC32",
    .checksum_type      = STC_CHECKSUM_DOUBLE_BYTE,
    .parity             = STC_PARITY_EVEN,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_64,
    .needs_freq_calib   = 1,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 1,
};

// USB15协议配置
static const stc_protocol_config_t stc_config_usb15 = {
    .name               = "USB15",
    .checksum_type      = STC_CHECKSUM_USB_BLOCK,
    .parity             = STC_PARITY_NONE,
    .brt_width          = STC_BRT_WIDTH_NONE,
    .block_size         = STC_BLOCK_SIZE_128,
    .needs_freq_calib   = 0,
    .option_bytes_len   = 5,
    .erase_countdown    = 0,
    .has_uid            = 1,
    .parity_switch      = 0,
    .bsl_magic_72       = 0,
};

#ifdef __cplusplus
}
#endif

#endif /* __STC_PROTOCOL_CONFIG_H__ */

