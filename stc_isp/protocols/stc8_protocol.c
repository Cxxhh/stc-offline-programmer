/**
 * @file stc8_protocol.c
 * @brief STC8系列协议实现（STC8/STC8d/STC8g/STC32）
 */

#include "stc8_protocol.h"
#include "../stc_packet.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 内部函数声明
 *============================================================================*/
static int send_packet(stc_context_t* ctx, const uint8_t* payload, uint16_t len);
static int recv_packet(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms);

/*============================================================================
 * 辅助函数
 *============================================================================*/
static int send_packet(stc_context_t* ctx, const uint8_t* payload, uint16_t len)
{
    if (ctx == NULL || ctx->hal == NULL || ctx->config == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    int pkt_len = stc_build_packet(ctx->config, payload, len, 
                                    ctx->tx_buffer, sizeof(ctx->tx_buffer));
    if (pkt_len < 0) {
        return STC_ERR_FRAME;
    }
    
    int ret = ctx->hal->write(ctx->uart_handle, ctx->tx_buffer, pkt_len, 
                               ctx->comm_config.timeout_ms);
    if (ret < 0) {
        return STC_ERR_TIMEOUT;
    }
    
    return STC_OK;
}

static int recv_packet(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms)
{
    if (ctx == NULL || ctx->hal == NULL || ctx->config == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    int rx_len = ctx->hal->read(ctx->uart_handle, ctx->rx_buffer, 
                                 sizeof(ctx->rx_buffer), timeout_ms);
    if (rx_len < 0) {
        return STC_ERR_TIMEOUT;
    }
    
    stc_packet_info_t info;
    int ret = stc_parse_packet(ctx->config, ctx->rx_buffer, rx_len, &info);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (payload != NULL && len != NULL) {
        memcpy(payload, info.payload, info.payload_len);
        *len = info.payload_len;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC8频率校准
 *============================================================================*/
int stc8_calibrate_frequency(stc_context_t* ctx, float target_freq)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    float user_speed = (target_freq > 0) ? target_freq : ctx->mcu_info.clock_hz;
    float program_speed = STC8_PROGRAM_FREQ;
    
    /* 计算目标频率计数 */
    uint32_t target_user_count = (uint32_t)(user_speed / (ctx->comm_config.baud_handshake / 2.0f) + 0.5f);
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /*========== 第一轮校准：测试分频器1-5 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;   /* 命令 */
    tx_buf[pos++] = 12;     /* 挑战数量 */
    
    /* 添加12个trim挑战 (23*1 到 23*10, 以及255) */
    for (int i = 1; i <= 10; i++) {
        tx_buf[pos++] = 23 * i;
        tx_buf[pos++] = 0x00;
    }
    tx_buf[pos++] = 255;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 255;
    tx_buf[pos++] = 0x00;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    
    /* 发送同步脉冲 (0xFE) */
    uint8_t sync = 0xFE;
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择合适的分频器 */
    uint8_t trim_divider = 0;
    uint16_t user_trim = 128;
    
    for (uint8_t divider = 1; divider <= 5; divider++) {
        uint32_t div_target = target_user_count * divider;
        
        /* 在响应中查找合适的范围 */
        for (int i = 0; i < 10; i++) {
            uint16_t count_a = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
            uint16_t count_b = (rx_buf[4 + 2*i] << 8) | rx_buf[5 + 2*i];
            
            if (count_a <= div_target && count_b >= div_target) {
                /* 线性插值计算trim值 */
                uint8_t trim_a = 23 * (i + 1);
                uint8_t trim_b = 23 * (i + 2);
                if (count_b != count_a) {
                    float m = (float)(trim_b - trim_a) / (count_b - count_a);
                    user_trim = (uint16_t)(trim_a + m * (div_target - count_a) + 0.5f);
                }
                trim_divider = divider;
                break;
            }
        }
        if (trim_divider > 0) break;
    }
    
    if (trim_divider == 0) {
        trim_divider = 1;
    }
    
    /*========== 第二轮校准：精细校准（±1范围，4个分频范围） ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 12;     /* 挑战数量 */
    
    /* 4个分频范围，每个范围3个trim值（±1） */
    for (uint8_t range = 0; range < 4; range++) {
        for (int8_t delta = -1; delta <= 1; delta++) {
            tx_buf[pos++] = (user_trim + delta) & 0xFF;
            tx_buf[pos++] = range;
        }
    }
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择最佳trim值 */
    uint16_t best_trim = user_trim;
    uint8_t best_range = 0;
    uint32_t best_count = 0;
    uint32_t best_diff = 0xFFFFFFFF;
    
    for (int i = 0; i < 12; i++) {
        uint16_t count = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
        uint32_t diff = (count > target_user_count) ? 
                        (count - target_user_count) : (target_user_count - count);
        
        if (diff < best_diff) {
            best_diff = diff;
            best_count = count;
            best_trim = tx_buf[2 + 2*i];
            best_range = tx_buf[3 + 2*i];
        }
    }
    
    /* 保存校准结果 */
    ctx->trim_result.user_trim = best_trim;
    ctx->trim_result.trim_range = best_range;
    ctx->trim_result.trim_divider = trim_divider;
    ctx->trim_result.final_frequency = (float)best_count * ctx->comm_config.baud_handshake / 2.0f / trim_divider;
    
    /*========== 切换波特率 ==========*/
    uint16_t brt = stc15_calc_brt(program_speed, ctx->comm_config.baud_transfer);
    uint8_t iap_wait = stc15_get_iap_delay(program_speed);
    
    pos = 0;
    tx_buf[pos++] = 0x01;   /* 波特率切换命令 */
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = best_range;     /* trim range */
    tx_buf[pos++] = best_trim;      /* trim value */
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 切换波特率 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    return STC_OK;
}

/*============================================================================
 * STC8d频率校准（特殊校准包格式）
 *============================================================================*/
int stc8d_calibrate_frequency(stc_context_t* ctx, float target_freq)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    float user_speed = (target_freq > 0) ? target_freq : ctx->mcu_info.clock_hz;
    float program_speed = STC8_PROGRAM_FREQ;
    
    uint32_t target_user_count = (uint32_t)(user_speed / (ctx->comm_config.baud_handshake / 2.0f) + 0.5f);
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /*========== 第一轮校准：4组trim挑战 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x08;   /* 8个挑战（4组） */
    
    /* 4组trim挑战 */
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x10;
    tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x10;
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x20;
    tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x20;
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x30;
    tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x30;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    
    uint8_t sync = 0xFE;
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择合适的trim范围 */
    uint8_t trim_range = 0;
    uint16_t user_trim = 128;
    
    for (int range = 0; range < 4; range++) {
        uint16_t count_min = (rx_buf[2 + 4*range] << 8) | rx_buf[3 + 4*range];
        uint16_t count_max = (rx_buf[4 + 4*range] << 8) | rx_buf[5 + 4*range];
        
        if (count_min <= target_user_count && count_max >= target_user_count) {
            trim_range = range * 0x10;
            /* 线性插值 */
            if (count_max != count_min) {
                float ratio = (float)(target_user_count - count_min) / (count_max - count_min);
                user_trim = (uint16_t)(ratio * 255 + 0.5f);
            }
            break;
        }
    }
    
    /*========== 第二轮校准：12个trim值（±6范围） ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x0C;   /* 12个挑战 */
    
    int16_t trim_start = (int16_t)user_trim - 6;
    if (trim_start < 0) trim_start = 0;
    
    for (int i = 0; i < 12; i++) {
        uint8_t trim_val = (trim_start + i) & 0xFF;
        tx_buf[pos++] = trim_val;
        tx_buf[pos++] = trim_range;
    }
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择最佳trim值 */
    uint16_t best_trim = user_trim;
    uint32_t best_diff = 0xFFFFFFFF;
    uint32_t best_count = 0;
    
    for (int i = 0; i < 12; i++) {
        uint16_t count = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
        uint32_t diff = (count > target_user_count) ? 
                        (count - target_user_count) : (target_user_count - count);
        
        if (diff < best_diff) {
            best_diff = diff;
            best_count = count;
            best_trim = trim_start + i;
        }
    }
    
    /* 保存校准结果 */
    ctx->trim_result.user_trim = best_trim;
    ctx->trim_result.trim_range = trim_range;
    ctx->trim_result.trim_divider = 1;
    ctx->trim_result.final_frequency = (float)best_count * ctx->comm_config.baud_handshake / 2.0f;
    
    /*========== 切换波特率 ==========*/
    uint16_t brt = stc15_calc_brt(program_speed, ctx->comm_config.baud_transfer);
    uint8_t iap_wait = stc15_get_iap_delay(program_speed);
    
    pos = 0;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = trim_range;
    tx_buf[pos++] = best_trim;
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    return STC_OK;
}

/*============================================================================
 * STC8g频率校准（需要epilogue）
 *============================================================================*/
int stc8g_calibrate_frequency(stc_context_t* ctx, float target_freq)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    float user_speed = (target_freq > 0) ? target_freq : ctx->mcu_info.clock_hz;
    float program_speed = STC8_PROGRAM_FREQ;
    
    uint32_t target_user_count = (uint32_t)(user_speed / (ctx->comm_config.baud_handshake / 2.0f) + 0.5f);
    
    uint8_t tx_buf[80];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /*========== 第一轮校准（需要epilogue） ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x05;   /* 5个挑战 */
    
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x80;
    tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x80;
    tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x00;
    
    /* 添加12字节的epilogue (0x66) */
    for (int i = 0; i < 12; i++) {
        tx_buf[pos++] = 0x66;
    }
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    
    uint8_t sync = 0xFE;
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 分析响应选择trim范围 */
    uint8_t trim_range = 0;
    uint16_t user_trim = 64;
    
    /* 检查两个范围 (0x00和0x80) */
    uint16_t count_00_min = (rx_buf[2] << 8) | rx_buf[3];
    uint16_t count_00_max = (rx_buf[4] << 8) | rx_buf[5];
    uint16_t count_80_min = (rx_buf[6] << 8) | rx_buf[7];
    uint16_t count_80_max = (rx_buf[8] << 8) | rx_buf[9];
    
    if (count_00_min <= target_user_count && count_00_max >= target_user_count) {
        trim_range = 0x00;
        if (count_00_max != count_00_min) {
            float ratio = (float)(target_user_count - count_00_min) / (count_00_max - count_00_min);
            user_trim = (uint16_t)(ratio * 128 + 0.5f);
        }
    } else if (count_80_min <= target_user_count && count_80_max >= target_user_count) {
        trim_range = 0x80;
        if (count_80_max != count_80_min) {
            float ratio = (float)(target_user_count - count_80_min) / (count_80_max - count_80_min);
            user_trim = (uint16_t)(ratio * 128 + 0.5f);
        }
    }
    
    /*========== 第二轮校准（也需要epilogue） ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x0C;   /* 12个挑战 */
    
    int16_t trim_start = (int16_t)user_trim - 6;
    if (trim_start < 0) trim_start = 0;
    
    for (int i = 0; i < 12; i++) {
        uint8_t trim_val = (trim_start + i) & 0xFF;
        tx_buf[pos++] = trim_val;
        tx_buf[pos++] = trim_range;
    }
    
    /* 添加19字节的epilogue */
    for (int i = 0; i < 19; i++) {
        tx_buf[pos++] = 0x66;
    }
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    for (int i = 0; i < 1000; i++) {
        ctx->hal->write(ctx->uart_handle, &sync, 1, 10);
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择最佳trim值 */
    uint16_t best_trim = user_trim;
    uint32_t best_diff = 0xFFFFFFFF;
    uint32_t best_count = 0;
    
    for (int i = 0; i < 12; i++) {
        uint16_t count = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
        uint32_t diff = (count > target_user_count) ? 
                        (count - target_user_count) : (target_user_count - count);
        
        if (diff < best_diff) {
            best_diff = diff;
            best_count = count;
            best_trim = trim_start + i;
        }
    }
    
    /* 保存校准结果 */
    ctx->trim_result.user_trim = best_trim;
    ctx->trim_result.trim_range = trim_range;
    ctx->trim_result.trim_divider = 1;
    ctx->trim_result.final_frequency = (float)best_count * ctx->comm_config.baud_handshake / 2.0f;
    
    /*========== 切换波特率 ==========*/
    uint16_t brt = stc15_calc_brt(program_speed, ctx->comm_config.baud_transfer);
    uint8_t iap_wait = stc15_get_iap_delay(program_speed);
    
    pos = 0;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = trim_range;
    tx_buf[pos++] = best_trim;
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    return STC_OK;
}

/*============================================================================
 * STC8设置选项字节（40字节格式）
 *============================================================================*/
int stc8_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    if (ctx == NULL || options == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    
    /* 构建40字节选项包 */
    uint8_t option_packet[40];
    memset(option_packet, 0xFF, sizeof(option_packet));
    
    /* 设置固定字段 */
    option_packet[3] = 0x00;
    option_packet[6] = 0x00;
    option_packet[22] = 0x00;
    
    /* 填充trim频率（4字节，大端序） */
    uint32_t trim_freq = (uint32_t)ctx->trim_result.final_frequency;
    option_packet[24] = (trim_freq >> 24) & 0xFF;
    option_packet[25] = (trim_freq >> 16) & 0xFF;
    option_packet[26] = (trim_freq >> 8) & 0xFF;
    option_packet[27] = trim_freq & 0xFF;
    
    /* 填充trim值 */
    option_packet[28] = (ctx->trim_result.user_trim >> 8) & 0xFF;
    option_packet[29] = ctx->trim_result.user_trim & 0xFF;
    option_packet[30] = ctx->trim_result.trim_divider;
    
    /* 填充用户选项 */
    if (len > 0) {
        option_packet[32] = options[0];     /* MSR[0] */
    }
    if (len > 1) {
        memcpy(&option_packet[36], &options[1], MIN(len - 1, 4));
    }
    
    /* 构建发送包 */
    uint16_t pos = 0;
    tx_buf[pos++] = STC_CMD_SET_OPTIONS;
    memcpy(&tx_buf[pos], option_packet, sizeof(option_packet));
    pos += sizeof(option_packet);
    
    int ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    return STC_OK;
}

/*============================================================================
 * 协议操作表
 *============================================================================*/

/* STC8标准版 */
const stc_protocol_ops_t stc8_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc8_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc8_set_options,
    .disconnect = stc15_disconnect,
};

/* STC8d (STC8H) */
const stc_protocol_ops_t stc8d_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc8d_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc8_set_options,
    .disconnect = stc15_disconnect,
};

/* STC8g (STC8H1K) */
const stc_protocol_ops_t stc8g_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc8g_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc8_set_options,
    .disconnect = stc15_disconnect,
};

/* STC32 (使用STC8d协议) */
const stc_protocol_ops_t stc32_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc8d_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc8_set_options,
    .disconnect = stc15_disconnect,
};

