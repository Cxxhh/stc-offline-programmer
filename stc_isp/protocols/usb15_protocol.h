/**
 * @file usb15_protocol.h
 * @brief STC15 USB协议实现（存根）
 * 
 * 注意：USB协议需要USB驱动支持，此文件为存根实现
 */

#ifndef __USB15_PROTOCOL_H__
#define __USB15_PROTOCOL_H__

#include "../stc_types.h"
#include "../stc_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * USB协议常量
 *============================================================================*/
#define USB15_VID           0x5354      // STC USB Vendor ID
#define USB15_PID           0x4312      // STC USB Product ID
#define USB15_BLOCK_SIZE    128         // USB块大小

/*============================================================================
 * USB15协议操作声明
 *============================================================================*/
extern const stc_protocol_ops_t usb15_protocol_ops;

/*============================================================================
 * USB15协议函数（存根）
 *============================================================================*/

/**
 * @brief USB15握手
 * @note 需要USB驱动支持
 */
int usb15_handshake(stc_context_t* ctx);

/**
 * @brief USB15擦除Flash
 */
int usb15_erase_flash(stc_context_t* ctx, uint32_t size);

/**
 * @brief USB15编程单个数据块
 */
int usb15_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first);

/**
 * @brief USB15设置选项字节
 */
int usb15_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len);

/**
 * @brief USB15断开连接
 */
int usb15_disconnect(stc_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* __USB15_PROTOCOL_H__ */

