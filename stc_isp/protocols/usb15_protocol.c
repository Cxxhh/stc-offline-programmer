/**
 * @file usb15_protocol.c
 * @brief STC15 USB协议实现（存根）
 * 
 * 注意：USB协议需要USB驱动支持
 * 此文件为存根实现，实际使用需要根据USB库进行适配
 */

#include "usb15_protocol.h"
#include "../stc_packet.h"
#include <string.h>

/*============================================================================
 * 存根实现 - 返回不支持错误
 *============================================================================*/

static int usb15_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len)
{
    (void)ctx;
    (void)data;
    (void)len;
    return STC_ERR_PROTOCOL;  /* USB协议暂不支持 */
}

int usb15_handshake(stc_context_t* ctx)
{
    (void)ctx;
    return STC_ERR_PROTOCOL;
}

int usb15_erase_flash(stc_context_t* ctx, uint32_t size)
{
    (void)ctx;
    (void)size;
    return STC_ERR_PROTOCOL;
}

int usb15_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, uint16_t len, uint8_t is_first)
{
    (void)ctx;
    (void)addr;
    (void)data;
    (void)len;
    (void)is_first;
    return STC_ERR_PROTOCOL;
}

int usb15_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    (void)ctx;
    (void)options;
    (void)len;
    return STC_ERR_PROTOCOL;
}

int usb15_disconnect(stc_context_t* ctx)
{
    (void)ctx;
    return STC_OK;
}

/*============================================================================
 * 协议操作表
 *============================================================================*/
const stc_protocol_ops_t usb15_protocol_ops = {
    .parse_status_packet = usb15_parse_status_packet,
    .handshake = usb15_handshake,
    .calibrate_frequency = NULL,
    .erase_flash = usb15_erase_flash,
    .program_block = usb15_program_block,
    .program_finish = NULL,
    .set_options = usb15_set_options,
    .disconnect = usb15_disconnect,
};

/*============================================================================
 * USB数据包构建（供将来实现参考）
 *============================================================================
 * 
 * USB数据包格式：每7字节数据+1字节校验和
 * 
 * int build_usb_packet(const uint8_t* data, uint16_t len, uint8_t* output)
 * {
 *     uint16_t out_pos = 0;
 *     uint16_t in_pos = 0;
 *     
 *     while (in_pos < len) {
 *         uint16_t chunk_len = MIN(7, len - in_pos);
 *         
 *         // 复制数据
 *         memcpy(&output[out_pos], &data[in_pos], chunk_len);
 *         out_pos += chunk_len;
 *         
 *         // 计算校验和（减法校验）
 *         uint8_t checksum = 0;
 *         for (int i = 0; i < chunk_len; i++) {
 *             checksum = (checksum - data[in_pos + i]) & 0xFF;
 *         }
 *         output[out_pos++] = checksum;
 *         
 *         in_pos += chunk_len;
 *     }
 *     
 *     return out_pos;
 * }
 * 
 * USB控制传输：
 * - 握手: bRequest=0x01, wValue=0, wIndex=0
 * - 解锁: bRequest=0x05, wValue=0xA55A, wIndex=0
 * - 擦除: bRequest=0x03, wValue=0xA55A, wIndex=0
 * - 首块写入: bRequest=0x22, wValue=0xA55A, wIndex=addr
 * - 后续写入: bRequest=0x02, wValue=0xA55A, wIndex=addr
 * - 设置选项: bRequest=0x04, wValue=0xA55A, wIndex=0
 * - 断开: bRequest=0xFF, wValue=0xA55A, wIndex=0
 * 
 *============================================================================*/

