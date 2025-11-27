/**
 * @file stc_types.h
 * @brief STC烧录器基础类型定义
 */

#ifndef __STC_TYPES_H__
#define __STC_TYPES_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 错误码定义
 *============================================================================*/
typedef enum {
    STC_OK = 0,
    STC_ERR_TIMEOUT = -1,
    STC_ERR_CHECKSUM = -2,
    STC_ERR_FRAME = -3,
    STC_ERR_PROTOCOL = -4,
    STC_ERR_UNKNOWN_MODEL = -5,
    STC_ERR_ERASE_FAIL = -6,
    STC_ERR_PROGRAM_FAIL = -7,
    STC_ERR_VERIFY_FAIL = -8,
    STC_ERR_HANDSHAKE_FAIL = -9,
    STC_ERR_CALIBRATION_FAIL = -10,
    STC_ERR_INVALID_PARAM = -11,
    STC_ERR_NO_RESPONSE = -12,
    STC_ERR_MCU_LOCKED = -13,
} stc_error_t;

/*============================================================================
 * 校验和类型
 *============================================================================*/
typedef enum {
    STC_CHECKSUM_SINGLE_BYTE,   // STC89: sum & 0xFF
    STC_CHECKSUM_DOUBLE_BYTE,   // STC89A/12+: sum & 0xFFFF
    STC_CHECKSUM_USB_BLOCK,     // USB: 每7字节一组
} stc_checksum_type_t;

/*============================================================================
 * 串口校验位类型
 *============================================================================*/
typedef enum {
    STC_PARITY_NONE,            // STC89
    STC_PARITY_EVEN,            // STC12+
} stc_parity_t;

/*============================================================================
 * BRT寄存器宽度
 *============================================================================*/
typedef enum {
    STC_BRT_WIDTH_8BIT,         // STC12: 256 - x
    STC_BRT_WIDTH_16BIT,        // STC89/89A: 65536 - x
    STC_BRT_WIDTH_NONE,         // STC15+: 使用trim值
} stc_brt_width_t;

/*============================================================================
 * 协议选择模式
 *============================================================================*/
typedef enum {
    STC_SELECT_AUTO,            // 自动识别（握手后根据Magic值）
    STC_SELECT_MANUAL,          // 手动选择（用户指定协议ID）
} stc_select_mode_t;

/*============================================================================
 * 协议ID枚举
 *============================================================================*/
typedef enum {
    STC_PROTO_STC89 = 0,        // STC89/90系列
    STC_PROTO_STC89A,           // STC89A系列
    STC_PROTO_STC12,            // STC10/11/12系列
    STC_PROTO_STC15A,           // STC15A系列
    STC_PROTO_STC15,            // STC15系列
    STC_PROTO_STC8,             // STC8系列
    STC_PROTO_STC8D,            // STC8H系列
    STC_PROTO_STC8G,            // STC8H1K系列
    STC_PROTO_STC32,            // STC32系列
    STC_PROTO_USB15,            // STC15 USB
    STC_PROTO_COUNT,            // 协议总数
} stc_protocol_id_t;

/*============================================================================
 * 数据包帧常量
 *============================================================================*/
#define STC_FRAME_START1        0x46
#define STC_FRAME_START2        0xB9
#define STC_FRAME_DIR_HOST      0x6A    // Host -> MCU
#define STC_FRAME_DIR_MCU       0x68    // MCU -> Host
#define STC_FRAME_END           0x16
#define STC_SYNC_CHAR           0x7F

/*============================================================================
 * 命令码定义
 *============================================================================*/
#define STC_CMD_STATUS          0x00
#define STC_CMD_WRITE_BLOCK     0x02
#define STC_CMD_ERASE           0x03
#define STC_CMD_SET_OPTIONS     0x04
#define STC_CMD_PREPARE         0x05
#define STC_CMD_FINISH_72       0x07    // BSL 7.2+
#define STC_CMD_WRITE_FIRST     0x22    // STC15+ 首块
#define STC_CMD_HANDSHAKE_REQ   0x50
#define STC_CMD_FREQ_CALIB      0x65    // STC15A频率校准
#define STC_CMD_FINISH          0x69    // STC12完成
#define STC_CMD_PING            0x80
#define STC_CMD_DISCONNECT      0x82
#define STC_CMD_ERASE_84        0x84
#define STC_CMD_SET_OPTIONS_8D  0x8D    // STC89设置选项
#define STC_CMD_BAUD_SWITCH     0x8E
#define STC_CMD_BAUD_TEST       0x8F
#define STC_CMD_DISCONNECT_FF   0xFF    // STC89A断开

/*============================================================================
 * 默认参数
 *============================================================================*/
#define STC_DEFAULT_BAUD_HANDSHAKE  2400
#define STC_DEFAULT_BAUD_TRANSFER   115200
#define STC_DEFAULT_TIMEOUT_MS      1000
#define STC_ERASE_TIMEOUT_MS        15000

#define STC_BLOCK_SIZE_128          128
#define STC_BLOCK_SIZE_64           64

#define STC_MAX_PACKET_SIZE         512
#define STC_MAX_PAYLOAD_SIZE        256
#define STC_UID_SIZE                7

/*============================================================================
 * 进度回调函数类型
 *============================================================================*/
typedef void (*stc_progress_cb_t)(uint32_t current, uint32_t total, void* user_data);

/*============================================================================
 * 日志回调函数类型
 *============================================================================*/
typedef void (*stc_log_cb_t)(const char* message, void* user_data);

/*============================================================================
 * 工具宏
 *============================================================================*/
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STC_TYPES_H__ */

