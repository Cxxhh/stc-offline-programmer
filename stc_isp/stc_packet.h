/**
 * @file stc_packet.h
 * @brief STC数据包构建和解析
 */

#ifndef __STC_PACKET_H__
#define __STC_PACKET_H__

#include "stc_types.h"
#include "stc_protocol_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 校验和计算
 *============================================================================*/

/**
 * @brief 计算单字节校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和（8位）
 */
uint8_t stc_checksum_8bit(const uint8_t* data, uint16_t len);

/**
 * @brief 计算双字节校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和（16位）
 */
uint16_t stc_checksum_16bit(const uint8_t* data, uint16_t len);

/**
 * @brief 计算USB块校验和（每7字节一组，减法校验）
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */
uint8_t stc_checksum_usb_block(const uint8_t* data, uint16_t len);

/**
 * @brief 根据配置计算校验和
 * @param config 协议配置
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */
uint16_t stc_calc_checksum(const stc_protocol_config_t* config, const uint8_t* data, uint16_t len);

/*============================================================================
 * 数据包构建
 *============================================================================*/

/**
 * @brief 构建写入数据包（Host -> MCU）
 * @param config 协议配置
 * @param payload 载荷数据
 * @param payload_len 载荷长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 数据包总长度，<0失败
 */
int stc_build_packet(const stc_protocol_config_t* config,
                     const uint8_t* payload, uint16_t payload_len,
                     uint8_t* output, uint16_t output_size);

/**
 * @brief 构建USB数据包（每7字节+1校验）
 * @param payload 载荷数据
 * @param payload_len 载荷长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 数据包总长度，<0失败
 */
int stc_build_usb_packet(const uint8_t* payload, uint16_t payload_len,
                         uint8_t* output, uint16_t output_size);

/*============================================================================
 * 数据包解析
 *============================================================================*/

/**
 * @brief 数据包解析结果
 */
typedef struct {
    const uint8_t*  payload;        // 载荷数据指针
    uint16_t        payload_len;    // 载荷长度
    uint8_t         direction;      // 方向标志
    uint16_t        checksum;       // 校验和
    uint8_t         checksum_valid; // 校验和是否有效
} stc_packet_info_t;

/**
 * @brief 解析接收数据包（MCU -> Host）
 * @param config 协议配置
 * @param data 接收数据
 * @param len 数据长度
 * @param info 解析结果输出
 * @return STC_OK成功，其他失败
 */
int stc_parse_packet(const stc_protocol_config_t* config,
                     const uint8_t* data, uint16_t len,
                     stc_packet_info_t* info);

/**
 * @brief 解析USB接收数据包
 * @param data 接收数据
 * @param len 数据长度
 * @param payload 载荷输出缓冲区
 * @param payload_size 载荷缓冲区大小
 * @return 载荷长度，<0失败
 */
int stc_parse_usb_packet(const uint8_t* data, uint16_t len,
                         uint8_t* payload, uint16_t payload_size);

/*============================================================================
 * 数据包接收（状态机）
 *============================================================================*/

/**
 * @brief 接收状态
 */
typedef enum {
    STC_RX_STATE_IDLE,              // 空闲，等待帧起始
    STC_RX_STATE_START1,            // 收到0x46，等待0xB9
    STC_RX_STATE_DIR,               // 等待方向字节
    STC_RX_STATE_LEN_H,             // 等待长度高字节
    STC_RX_STATE_LEN_L,             // 等待长度低字节
    STC_RX_STATE_PAYLOAD,           // 接收载荷
    STC_RX_STATE_CHECKSUM,          // 接收校验和
    STC_RX_STATE_END,               // 等待帧结束
    STC_RX_STATE_COMPLETE,          // 接收完成
    STC_RX_STATE_ERROR,             // 错误
} stc_rx_state_t;

/**
 * @brief 接收状态机上下文
 */
typedef struct {
    stc_rx_state_t  state;          // 当前状态
    uint8_t*        buffer;         // 接收缓冲区
    uint16_t        buffer_size;    // 缓冲区大小
    uint16_t        index;          // 当前写入位置
    uint16_t        expected_len;   // 期望长度
    uint8_t         checksum_bytes; // 校验和字节数
} stc_rx_context_t;

/**
 * @brief 初始化接收状态机
 * @param rx_ctx 接收上下文
 * @param buffer 接收缓冲区
 * @param buffer_size 缓冲区大小
 * @param checksum_double 是否使用双字节校验和
 */
void stc_rx_init(stc_rx_context_t* rx_ctx, uint8_t* buffer, uint16_t buffer_size, uint8_t checksum_double);

/**
 * @brief 重置接收状态机
 * @param rx_ctx 接收上下文
 */
void stc_rx_reset(stc_rx_context_t* rx_ctx);

/**
 * @brief 处理接收字节
 * @param rx_ctx 接收上下文
 * @param byte 接收到的字节
 * @return 状态：STC_RX_STATE_COMPLETE表示完成，STC_RX_STATE_ERROR表示错误
 */
stc_rx_state_t stc_rx_process_byte(stc_rx_context_t* rx_ctx, uint8_t byte);

/**
 * @brief 获取接收数据长度
 * @param rx_ctx 接收上下文
 * @return 接收数据长度
 */
uint16_t stc_rx_get_length(stc_rx_context_t* rx_ctx);

#ifdef __cplusplus
}
#endif

#endif /* __STC_PACKET_H__ */

