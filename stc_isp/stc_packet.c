/**
 * @file stc_packet.c
 * @brief STC数据包构建和解析实现
 */

#include "stc_packet.h"
#include <string.h>

/*============================================================================
 * 校验和计算
 *============================================================================*/

uint8_t stc_checksum_8bit(const uint8_t* data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

uint16_t stc_checksum_16bit(const uint8_t* data, uint16_t len)
{
    uint16_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

uint8_t stc_checksum_usb_block(const uint8_t* data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum = (sum - data[i]) & 0xFF;
    }
    return sum;
}

uint16_t stc_calc_checksum(const stc_protocol_config_t* config, const uint8_t* data, uint16_t len)
{
    if (config == NULL || data == NULL) {
        return 0;
    }
    
    switch (config->checksum_type) {
        case STC_CHECKSUM_SINGLE_BYTE:
            return stc_checksum_8bit(data, len);
            
        case STC_CHECKSUM_DOUBLE_BYTE:
            return stc_checksum_16bit(data, len);
            
        case STC_CHECKSUM_USB_BLOCK:
            return stc_checksum_usb_block(data, len);
            
        default:
            return 0;
    }
}

/*============================================================================
 * 数据包构建
 *============================================================================*/

int stc_build_packet(const stc_protocol_config_t* config,
                     const uint8_t* payload, uint16_t payload_len,
                     uint8_t* output, uint16_t output_size)
{
    if (config == NULL || payload == NULL || output == NULL) {
        return -1;
    }
    
    /* 计算校验和字节数 */
    uint8_t checksum_bytes = (config->checksum_type == STC_CHECKSUM_DOUBLE_BYTE) ? 2 : 1;
    
    /* 计算总长度：帧头(2) + 方向(1) + 长度(2) + 载荷 + 校验和 + 帧尾(1) */
    uint16_t total_len = 2 + 1 + 2 + payload_len + checksum_bytes + 1;
    
    if (output_size < total_len) {
        return -2;  /* 缓冲区不足 */
    }
    
    uint16_t pos = 0;
    
    /* 帧起始 */
    output[pos++] = STC_FRAME_START1;   /* 0x46 */
    output[pos++] = STC_FRAME_START2;   /* 0xB9 */
    
    /* 方向标志（Host -> MCU） */
    output[pos++] = STC_FRAME_DIR_HOST; /* 0x6A */
    
    /* 数据长度（大端序）：长度字段包含方向(1) + 长度本身(2) + 载荷 + 校验和 */
    uint16_t len_field = 1 + 2 + payload_len + checksum_bytes;
    output[pos++] = (len_field >> 8) & 0xFF;
    output[pos++] = len_field & 0xFF;
    
    /* 数据载荷 */
    memcpy(&output[pos], payload, payload_len);
    pos += payload_len;
    
    /* 计算校验和（从方向字节开始到载荷结束） */
    uint16_t checksum = stc_calc_checksum(config, &output[2], pos - 2);
    
    /* 添加校验和 */
    if (checksum_bytes == 2) {
        output[pos++] = (checksum >> 8) & 0xFF;
        output[pos++] = checksum & 0xFF;
    } else {
        output[pos++] = checksum & 0xFF;
    }
    
    /* 帧结束 */
    output[pos++] = STC_FRAME_END;      /* 0x16 */
    
    return pos;
}

int stc_build_usb_packet(const uint8_t* payload, uint16_t payload_len,
                         uint8_t* output, uint16_t output_size)
{
    if (payload == NULL || output == NULL) {
        return -1;
    }
    
    /* 每7字节数据 + 1字节校验，计算所需空间 */
    uint16_t num_blocks = (payload_len + 6) / 7;  /* 向上取整 */
    uint16_t total_len = payload_len + num_blocks;
    
    if (output_size < total_len) {
        return -2;
    }
    
    uint16_t out_pos = 0;
    uint16_t in_pos = 0;
    
    while (in_pos < payload_len) {
        /* 计算当前块大小（最多7字节） */
        uint16_t block_len = payload_len - in_pos;
        if (block_len > 7) {
            block_len = 7;
        }
        
        /* 复制数据 */
        memcpy(&output[out_pos], &payload[in_pos], block_len);
        out_pos += block_len;
        
        /* 计算并添加校验和（减法校验） */
        uint8_t checksum = stc_checksum_usb_block(&payload[in_pos], block_len);
        output[out_pos++] = checksum;
        
        in_pos += block_len;
    }
    
    return out_pos;
}

/*============================================================================
 * 数据包解析
 *============================================================================*/

int stc_parse_packet(const stc_protocol_config_t* config,
                     const uint8_t* data, uint16_t len,
                     stc_packet_info_t* info)
{
    if (config == NULL || data == NULL || info == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 最小长度检查：帧头(2) + 方向(1) + 长度(2) + 校验和(1-2) + 帧尾(1) */
    uint8_t checksum_bytes = (config->checksum_type == STC_CHECKSUM_DOUBLE_BYTE) ? 2 : 1;
    uint16_t min_len = 2 + 1 + 2 + checksum_bytes + 1;
    
    if (len < min_len) {
        return STC_ERR_FRAME;
    }
    
    /* 检查帧起始 */
    if (data[0] != STC_FRAME_START1 || data[1] != STC_FRAME_START2) {
        return STC_ERR_FRAME;
    }
    
    /* 获取方向标志 */
    info->direction = data[2];
    
    /* 获取长度字段 */
    uint16_t len_field = (data[3] << 8) | data[4];
    
    /* 验证总长度 */
    uint16_t expected_total = 2 + len_field + 1;  /* 帧头 + 长度字段内容 + 帧尾 */
    if (len < expected_total) {
        return STC_ERR_FRAME;
    }
    
    /* 检查帧结束 */
    if (data[expected_total - 1] != STC_FRAME_END) {
        return STC_ERR_FRAME;
    }
    
    /* 计算载荷长度 */
    info->payload_len = len_field - 1 - 2 - checksum_bytes;  /* 减去方向、长度本身、校验和 */
    info->payload = &data[5];  /* 载荷从第6字节开始 */
    
    /* 获取接收到的校验和 */
    uint16_t checksum_pos = 5 + info->payload_len;
    if (checksum_bytes == 2) {
        info->checksum = (data[checksum_pos] << 8) | data[checksum_pos + 1];
    } else {
        info->checksum = data[checksum_pos];
    }
    
    /* 计算并验证校验和 */
    uint16_t calc_checksum = stc_calc_checksum(config, &data[2], 3 + info->payload_len);
    info->checksum_valid = (calc_checksum == info->checksum) ? 1 : 0;
    
    if (!info->checksum_valid) {
        return STC_ERR_CHECKSUM;
    }
    
    return STC_OK;
}

int stc_parse_usb_packet(const uint8_t* data, uint16_t len,
                         uint8_t* payload, uint16_t payload_size)
{
    if (data == NULL || payload == NULL) {
        return -1;
    }
    
    /* 检查最小长度 */
    if (len < 5) {
        return -2;
    }
    
    /* 检查帧起始 */
    if (data[0] != STC_FRAME_START1 || data[1] != STC_FRAME_START2) {
        return -3;
    }
    
    /* 获取数据长度 */
    uint8_t data_len = data[2];
    
    if (data_len > payload_size) {
        return -4;
    }
    
    /* 验证校验和（减法校验） */
    uint8_t checksum = 0;
    for (uint16_t i = 2; i < len - 1; i++) {
        checksum = (checksum - data[i]) & 0xFF;
    }
    
    if (checksum != data[len - 1]) {
        return -5;  /* 校验和错误 */
    }
    
    /* 提取载荷 */
    memcpy(payload, &data[3], data_len);
    
    return data_len;
}

/*============================================================================
 * 数据包接收状态机
 *============================================================================*/

void stc_rx_init(stc_rx_context_t* rx_ctx, uint8_t* buffer, uint16_t buffer_size, uint8_t checksum_double)
{
    if (rx_ctx == NULL) {
        return;
    }
    
    rx_ctx->buffer = buffer;
    rx_ctx->buffer_size = buffer_size;
    rx_ctx->checksum_bytes = checksum_double ? 2 : 1;
    stc_rx_reset(rx_ctx);
}

void stc_rx_reset(stc_rx_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        return;
    }
    
    rx_ctx->state = STC_RX_STATE_IDLE;
    rx_ctx->index = 0;
    rx_ctx->expected_len = 0;
}

stc_rx_state_t stc_rx_process_byte(stc_rx_context_t* rx_ctx, uint8_t byte)
{
    if (rx_ctx == NULL || rx_ctx->buffer == NULL) {
        return STC_RX_STATE_ERROR;
    }
    
    /* 存储字节 */
    if (rx_ctx->index < rx_ctx->buffer_size) {
        rx_ctx->buffer[rx_ctx->index++] = byte;
    } else {
        rx_ctx->state = STC_RX_STATE_ERROR;
        return rx_ctx->state;
    }
    
    switch (rx_ctx->state) {
        case STC_RX_STATE_IDLE:
            if (byte == STC_FRAME_START1) {
                rx_ctx->state = STC_RX_STATE_START1;
            } else {
                /* 丢弃非起始字节，重置索引 */
                rx_ctx->index = 0;
            }
            break;
            
        case STC_RX_STATE_START1:
            if (byte == STC_FRAME_START2) {
                rx_ctx->state = STC_RX_STATE_DIR;
            } else if (byte == STC_FRAME_START1) {
                /* 继续等待0xB9 */
                rx_ctx->index = 1;
            } else {
                /* 重置 */
                stc_rx_reset(rx_ctx);
            }
            break;
            
        case STC_RX_STATE_DIR:
            /* 方向字节，继续读长度高字节 */
            rx_ctx->state = STC_RX_STATE_LEN_H;
            break;
            
        case STC_RX_STATE_LEN_H:
            /* 长度高字节 */
            rx_ctx->expected_len = byte << 8;
            rx_ctx->state = STC_RX_STATE_LEN_L;
            break;
            
        case STC_RX_STATE_LEN_L:
            /* 长度低字节 */
            rx_ctx->expected_len |= byte;
            
            /* 长度字段包含：方向(1) + 长度(2) + 载荷 + 校验和
             * 剩余需要接收：载荷 + 校验和 + 帧尾(1)
             * 已接收：帧头(2) + 方向(1) + 长度(2) = 5字节
             * 需要接收的剩余字节 = expected_len - 3 + 1 (帧尾)
             */
            rx_ctx->state = STC_RX_STATE_PAYLOAD;
            break;
            
        case STC_RX_STATE_PAYLOAD:
            /* 检查是否接收完成 */
            /* 完整帧长度 = 帧头(2) + 长度字段内容 + 帧尾(1) */
            if (rx_ctx->index >= (2 + rx_ctx->expected_len + 1)) {
                /* 检查帧尾 */
                if (byte == STC_FRAME_END) {
                    rx_ctx->state = STC_RX_STATE_COMPLETE;
                } else {
                    rx_ctx->state = STC_RX_STATE_ERROR;
                }
            }
            break;
            
        case STC_RX_STATE_COMPLETE:
        case STC_RX_STATE_ERROR:
            /* 已完成或错误状态，不处理 */
            break;
            
        default:
            rx_ctx->state = STC_RX_STATE_ERROR;
            break;
    }
    
    return rx_ctx->state;
}

uint16_t stc_rx_get_length(stc_rx_context_t* rx_ctx)
{
    if (rx_ctx == NULL) {
        return 0;
    }
    return rx_ctx->index;
}

