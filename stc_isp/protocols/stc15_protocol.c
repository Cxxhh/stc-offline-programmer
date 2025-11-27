/**
 * @file stc15_protocol.c
 * @brief STC15系列协议实现（含频率校准）
 */

#include "stc15_protocol.h"
#include "../stc_packet.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 内部辅助函数声明
 *============================================================================*/
static int send_packet(stc_context_t* ctx, const uint8_t* payload, uint16_t len);
static int recv_packet(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms);
static int send_and_recv(stc_context_t* ctx, const uint8_t* tx_payload, uint16_t tx_len,
                         uint8_t* rx_payload, uint16_t* rx_len, uint32_t timeout_ms);

/*============================================================================
 * IAP等待状态计算
 *============================================================================*/
uint8_t stc15_get_iap_delay(float clock_hz)
{
    if (clock_hz < 1E6)  return 0x87;
    if (clock_hz < 2E6)  return 0x86;
    if (clock_hz < 3E6)  return 0x85;
    if (clock_hz < 6E6)  return 0x84;
    if (clock_hz < 12E6) return 0x83;
    if (clock_hz < 20E6) return 0x82;
    if (clock_hz < 24E6) return 0x81;
    return 0x80;
}

uint16_t stc15_calc_brt(float program_freq, uint32_t baud_transfer)
{
    return (uint16_t)(65536.0f - program_freq / (baud_transfer * 4.0f) + 0.5f);
}

/*============================================================================
 * 发送接收辅助函数
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
    
    /* 接收数据 */
    int rx_len = ctx->hal->read(ctx->uart_handle, ctx->rx_buffer, 
                                 sizeof(ctx->rx_buffer), timeout_ms);
    if (rx_len < 0) {
        return STC_ERR_TIMEOUT;
    }
    
    /* 解析数据包 */
    stc_packet_info_t info;
    int ret = stc_parse_packet(ctx->config, ctx->rx_buffer, rx_len, &info);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 复制载荷 */
    if (payload != NULL && len != NULL) {
        memcpy(payload, info.payload, info.payload_len);
        *len = info.payload_len;
    }
    
    return STC_OK;
}

static int send_and_recv(stc_context_t* ctx, const uint8_t* tx_payload, uint16_t tx_len,
                         uint8_t* rx_payload, uint16_t* rx_len, uint32_t timeout_ms)
{
    int ret = send_packet(ctx, tx_payload, tx_len);
    if (ret != STC_OK) {
        return ret;
    }
    
    return recv_packet(ctx, rx_payload, rx_len, timeout_ms);
}

/*============================================================================
 * 同步脉冲
 *============================================================================*/
int stc15_pulse_sync(stc_context_t* ctx, uint16_t count, uint16_t interval_ms)
{
    if (ctx == NULL || ctx->hal == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t sync_char = STC_SYNC_CHAR;
    
    for (uint16_t i = 0; i < count; i++) {
        ctx->hal->write(ctx->uart_handle, &sync_char, 1, 100);
        if (interval_ms > 0) {
            ctx->hal->delay_ms(interval_ms);
        }
    }
    
    return STC_OK;
}

/*============================================================================
 * 状态包解析
 *============================================================================*/
int stc15_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (ctx == NULL || data == NULL || len < 20) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 保存原始状态包 */
    memcpy(ctx->status_packet, data, MIN(len, sizeof(ctx->status_packet)));
    ctx->status_packet_len = len;
    
    /* 解析Magic值（通常在偏移20-21处） */
    if (len >= 22) {
        ctx->mcu_info.magic = (data[20] << 8) | data[21];
    }
    
    /* 解析频率计数器（8次测量的平均值） */
    uint32_t freq_sum = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t count = (data[1 + 2*i] << 8) | data[2 + 2*i];
        freq_sum += count;
    }
    ctx->mcu_info.freq_counter = freq_sum / 8;
    
    /* 解析BSL版本 */
    if (len >= 18) {
        ctx->mcu_info.bsl_version = data[17];
    }
    
    /* 计算MCU频率 */
    /* 频率 = 波特率 * 频率计数 * 12 / 7 */
    ctx->mcu_info.clock_hz = (float)ctx->comm_config.baud_handshake * 
                              ctx->mcu_info.freq_counter * 12.0f / 7.0f;
    
    return STC_OK;
}

/*============================================================================
 * 握手流程
 *============================================================================*/
int stc15_handshake(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* STC15系列在频率校准时完成波特率切换，这里只做基本握手 */
    uint8_t tx_buf[16];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    
    /* 发送握手请求 0x50 */
    tx_buf[0] = STC_CMD_HANDSHAKE_REQ;
    tx_buf[1] = 0x00;
    tx_buf[2] = 0x00;
    tx_buf[3] = 0x36;
    tx_buf[4] = 0x01;
    tx_buf[5] = (ctx->mcu_info.magic >> 8) & 0xFF;
    tx_buf[6] = ctx->mcu_info.magic & 0xFF;
    
    int ret = send_and_recv(ctx, tx_buf, 7, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 验证响应 */
    if (rx_len < 1 || rx_buf[0] != 0x8F) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 频率校准 (STC15)
 *============================================================================*/
int stc15_calibrate_frequency(stc_context_t* ctx, float target_freq)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 确定目标频率 */
    float user_speed = (target_freq > 0) ? target_freq : ctx->mcu_info.clock_hz;
    float program_speed = STC15_PROGRAM_FREQ_24M;  /* STC15使用24MHz */
    
    /* 计算目标频率计数 */
    uint32_t target_count = (uint32_t)(ctx->mcu_info.freq_counter * 
                                        (user_speed / ctx->mcu_info.clock_hz) + 0.5f);
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /*========== 第一轮校准：粗调 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;   /* 命令 */
    tx_buf[pos++] = 12;     /* 12个trim挑战 */
    
    /* 添加12个trim挑战 (23*1 到 23*10, 以及255) */
    for (int i = 1; i <= 10; i++) {
        tx_buf[pos++] = 23 * i;
        tx_buf[pos++] = 0x00;
    }
    tx_buf[pos++] = 255;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 255;  /* 额外的填充 */
    tx_buf[pos++] = 0x00;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 发送同步脉冲 */
    ctx->hal->delay_ms(100);
    stc15_pulse_sync(ctx, 1000, 0);
    
    /* 接收响应 */
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择合适的分频范围 */
    uint8_t trim_divider = 0;
    uint16_t user_trim = 0;
    
    /* 分析响应，找到合适的trim值 */
    for (uint8_t divider = 1; divider <= 5; divider++) {
        uint32_t div_target = target_count * divider;
        
        /* 在响应中查找范围 */
        for (int i = 0; i < 10; i++) {
            uint16_t count = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
            uint16_t next_count = (rx_buf[4 + 2*i] << 8) | rx_buf[5 + 2*i];
            
            if (count <= div_target && next_count >= div_target) {
                /* 线性插值 */
                uint8_t trim_a = 23 * (i + 1);
                uint8_t trim_b = 23 * (i + 2);
                float m = (float)(trim_b - trim_a) / (next_count - count);
                user_trim = (uint16_t)(trim_a + m * (div_target - count) + 0.5f);
                trim_divider = divider;
                break;
            }
        }
        if (trim_divider > 0) break;
    }
    
    if (trim_divider == 0) {
        /* 使用默认值 */
        user_trim = 128;
        trim_divider = 1;
    }
    
    /*========== 第二轮校准：精调 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 12;     /* 12个trim挑战 */
    
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
    stc15_pulse_sync(ctx, 1000, 0);
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 选择最佳trim值 */
    uint16_t best_trim = user_trim;
    uint8_t best_range = 0;
    uint32_t best_count = 0xFFFFFFFF;
    uint32_t best_diff = 0xFFFFFFFF;
    
    for (int i = 0; i < 12; i++) {
        uint16_t count = (rx_buf[2 + 2*i] << 8) | rx_buf[3 + 2*i];
        uint32_t diff = (count > target_count) ? (count - target_count) : (target_count - count);
        
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
    
    /* 计算编程频率的trim值 */
    uint32_t prog_target = (uint32_t)(ctx->mcu_info.freq_counter * 
                                       (program_speed / ctx->mcu_info.clock_hz) + 0.5f);
    /* 简化：使用与用户频率相同的比例 */
    ctx->trim_result.program_trim = (uint16_t)((float)best_trim * prog_target / best_count + 0.5f);
    
    /*========== 切换波特率 ==========*/
    uint16_t brt = stc15_calc_brt(program_speed, ctx->comm_config.baud_transfer);
    uint8_t iap_wait = stc15_get_iap_delay(program_speed);
    
    pos = 0;
    tx_buf[pos++] = 0x01;   /* 波特率切换命令 */
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = best_range;
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
    
    /* 切换波特率 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    return STC_OK;
}

/*============================================================================
 * STC15A频率校准（更复杂的两轮校准）
 *============================================================================*/
int stc15a_calibrate_frequency(stc_context_t* ctx, float target_freq)
{
    /* STC15A使用类似但更复杂的校准流程 */
    /* 主要区别：使用0x65命令，需要校准数据，以及不同的trim挑战序列 */
    
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    float user_speed = (target_freq > 0) ? target_freq : ctx->mcu_info.clock_hz;
    float program_speed = STC15_PROGRAM_FREQ;  /* 22.1184 MHz */
    
    uint32_t user_count = (uint32_t)(ctx->mcu_info.freq_counter * 
                                      (user_speed / ctx->mcu_info.clock_hz) + 0.5f);
    uint32_t program_count = (uint32_t)(ctx->mcu_info.freq_counter * 
                                         (program_speed / ctx->mcu_info.clock_hz) + 0.5f);
    
    uint8_t tx_buf[128];
    uint8_t rx_buf[128];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /*========== 第一轮校准 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x65;   /* 校准命令 */
    
    /* 添加校准数据（从状态包获取） */
    if (ctx->status_packet_len >= 25) {
        memcpy(&tx_buf[pos], &ctx->status_packet[18], 7);
        pos += 7;
    } else {
        memset(&tx_buf[pos], 0xFF, 7);
        pos += 7;
    }
    
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0x06;   /* 用户频率挑战数 */
    tx_buf[pos++] = 0x06;   /* 编程频率挑战数 */
    
    /* 根据频率选择trim挑战序列 */
    if (user_speed < 7.5E6) {
        tx_buf[pos++] = 0x18; tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x18; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x18; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x18; tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
    } else if (user_speed < 15E6) {
        tx_buf[pos++] = 0x58; tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x58; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x58; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x58; tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
    } else {
        tx_buf[pos++] = 0x98; tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x98; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x98; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x98; tx_buf[pos++] = 0xFF; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
    }
    
    /* 编程频率trim挑战 */
    tx_buf[pos++] = 0x98; tx_buf[pos++] = 0x00; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x98; tx_buf[pos++] = 0x80; tx_buf[pos++] = 0x02; tx_buf[pos++] = 0x00;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    stc15_pulse_sync(ctx, 1000, 0);
    
    ret = recv_packet(ctx, rx_buf, &rx_len, 2000);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 分析响应并选择trim值（简化处理） */
    uint16_t prog_trim = 0x9880;  /* 默认值 */
    uint16_t user_trim = 0x9840;
    
    /* 从响应中提取trim值（响应格式依赖于具体协议） */
    if (rx_len >= 40) {
        /* 编程频率trim值（线性插值） */
        uint16_t prog_trim_a = (rx_buf[28] << 8) | rx_buf[29];
        uint16_t prog_count_a = (rx_buf[30] << 8) | rx_buf[31];
        uint16_t prog_trim_b = (rx_buf[32] << 8) | rx_buf[33];
        uint16_t prog_count_b = (rx_buf[34] << 8) | rx_buf[35];
        
        if (prog_count_b != prog_count_a) {
            float m = (float)(prog_trim_b - prog_trim_a) / (prog_count_b - prog_count_a);
            float n = prog_trim_a - m * prog_count_a;
            prog_trim = (uint16_t)(m * program_count + n + 0.5f);
        }
        
        /* 用户频率trim值 */
        uint16_t user_trim_a = (rx_buf[12] << 8) | rx_buf[13];
        uint16_t user_count_a = (rx_buf[14] << 8) | rx_buf[15];
        uint16_t user_trim_b = (rx_buf[20] << 8) | rx_buf[21];
        uint16_t user_count_b = (rx_buf[22] << 8) | rx_buf[23];
        
        if (user_count_b != user_count_a) {
            float m = (float)(user_trim_b - user_trim_a) / (user_count_b - user_count_a);
            float n = user_trim_a - m * user_count_a;
            user_trim = (uint16_t)(m * user_count + n + 0.5f);
        }
    }
    
    /* 保存校准结果 */
    ctx->trim_result.user_trim = user_trim;
    ctx->trim_result.program_trim = prog_trim;
    ctx->trim_result.final_frequency = user_speed;
    
    /*========== 切换波特率 ==========*/
    uint8_t baud_div = (uint8_t)(230400 / ctx->comm_config.baud_transfer);
    uint8_t iap_wait = stc15_get_iap_delay(program_speed);
    
    pos = 0;
    tx_buf[pos++] = 0x8E;   /* 波特率切换命令 */
    tx_buf[pos++] = (prog_trim >> 8) & 0xFF;
    tx_buf[pos++] = prog_trim & 0xFF;
    tx_buf[pos++] = baud_div;
    tx_buf[pos++] = 0xA1;
    tx_buf[pos++] = 0x64;
    tx_buf[pos++] = 0xB8;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = iap_wait;
    tx_buf[pos++] = 0x20;
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0x00;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    return STC_OK;
}

/*============================================================================
 * 擦除Flash
 *============================================================================*/
int stc15_erase_flash(stc_context_t* ctx, uint32_t size)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    
    /* 发送擦除命令 0x03 */
    tx_buf[0] = STC_CMD_ERASE;
    tx_buf[1] = 0x00;   /* 擦除类型：0x00=仅Flash, 0x01=Flash+EEPROM */
    tx_buf[2] = 0x00;
    tx_buf[3] = 0x5A;   /* 魔术字 */
    tx_buf[4] = 0xA5;
    
    int ret = send_packet(ctx, tx_buf, 5);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 等待擦除完成（较长超时） */
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.erase_timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_ERASE_FAIL;
    }
    
    /* 验证响应 */
    if (rx_len < 1 || rx_buf[0] != STC_CMD_ERASE) {
        return STC_ERR_ERASE_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 编程单个数据块
 *============================================================================*/
int stc15_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, 
                        uint16_t len, uint8_t is_first)
{
    if (ctx == NULL || data == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[128];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    /* 命令字节 */
    tx_buf[pos++] = is_first ? STC_CMD_WRITE_FIRST : STC_CMD_WRITE_BLOCK;
    
    /* 地址（大端序） */
    tx_buf[pos++] = (addr >> 8) & 0xFF;
    tx_buf[pos++] = addr & 0xFF;
    
    /* BSL 7.2+需要魔术字 */
    if (ctx->config->bsl_magic_72) {
        tx_buf[pos++] = 0x5A;
        tx_buf[pos++] = 0xA5;
    }
    
    /* 数据 */
    memcpy(&tx_buf[pos], data, len);
    pos += len;
    
    /* 填充到块大小 */
    while (pos < ctx->config->block_size + 3 + (ctx->config->bsl_magic_72 ? 2 : 0)) {
        tx_buf[pos++] = 0x00;
    }
    
    int ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    /* 验证响应 */
    if (rx_len < 2 || rx_buf[0] != STC_CMD_WRITE_BLOCK || rx_buf[1] != 0x54) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 编程完成
 *============================================================================*/
int stc15_program_finish(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* BSL 7.2+需要发送完成命令 */
    if (!ctx->config->bsl_magic_72) {
        return STC_OK;  /* 旧版本不需要 */
    }
    
    uint8_t tx_buf[8];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    
    tx_buf[0] = STC_CMD_FINISH_72;
    tx_buf[1] = 0x00;
    tx_buf[2] = 0x00;
    tx_buf[3] = 0x5A;
    tx_buf[4] = 0xA5;
    
    int ret = send_packet(ctx, tx_buf, 5);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 2 || rx_buf[0] != STC_CMD_FINISH_72 || rx_buf[1] != 0x54) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 设置选项字节
 *============================================================================*/
int stc15_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    if (ctx == NULL || options == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_SET_OPTIONS;
    
    /* 添加选项字节 */
    memcpy(&tx_buf[pos], options, len);
    pos += len;
    
    /* 添加trim值 */
    tx_buf[pos++] = (ctx->trim_result.user_trim >> 8) & 0xFF;
    tx_buf[pos++] = ctx->trim_result.user_trim & 0xFF;
    
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
 * 断开连接
 *============================================================================*/
int stc15_disconnect(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[4];
    tx_buf[0] = STC_CMD_DISCONNECT;
    
    /* 发送断开命令，不等待响应 */
    send_packet(ctx, tx_buf, 1);
    
    return STC_OK;
}

/*============================================================================
 * 协议操作表
 *============================================================================*/
const stc_protocol_ops_t stc15_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc15_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc15_set_options,
    .disconnect = stc15_disconnect,
};

const stc_protocol_ops_t stc15a_protocol_ops = {
    .parse_status_packet = stc15_parse_status_packet,
    .handshake = stc15_handshake,
    .calibrate_frequency = stc15a_calibrate_frequency,
    .erase_flash = stc15_erase_flash,
    .program_block = stc15_program_block,
    .program_finish = stc15_program_finish,
    .set_options = stc15_set_options,
    .disconnect = stc15_disconnect,
};

