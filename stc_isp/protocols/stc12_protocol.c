/**
 * @file stc12_protocol.c
 * @brief STC12系列协议实现
 */

#include "stc12_protocol.h"
#include "../stc_packet.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 内部函数声明
 *============================================================================*/
static int send_packet(stc_context_t* ctx, const uint8_t* payload, uint16_t len);
static int recv_packet(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms);

/*============================================================================
 * 辅助函数实现
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
 * BRT和IAP计算
 *============================================================================*/
uint8_t stc12_calc_brt(float mcu_clock_hz, uint32_t baud_transfer)
{
    /* STC12: BRT = 256 - clock / (baud * 16) */
    int16_t brt = 256 - (int16_t)(mcu_clock_hz / (baud_transfer * 16.0f) + 0.5f);
    
    if (brt <= 1) brt = 1;
    if (brt > 255) brt = 255;
    
    return (uint8_t)brt;
}

uint8_t stc12_get_iap_delay(float clock_hz)
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

/*============================================================================
 * 状态包解析
 *============================================================================*/
int stc12_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (ctx == NULL || data == NULL || len < 20) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 保存原始状态包 */
    memcpy(ctx->status_packet, data, MIN(len, sizeof(ctx->status_packet)));
    ctx->status_packet_len = len;
    
    /* 解析Magic值 */
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
    /* STC12频率 = 波特率 * 频率计数 * 12 / 7 */
    ctx->mcu_info.clock_hz = (float)ctx->comm_config.baud_handshake * 
                              ctx->mcu_info.freq_counter * 12.0f / 7.0f;
    
    /* 默认12T模式 */
    ctx->mcu_info.cpu_6t = 0;
    
    return STC_OK;
}

/*============================================================================
 * 握手流程
 *============================================================================*/
int stc12_handshake(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos;
    int ret;
    
    /* 计算波特率参数 */
    uint8_t brt = stc12_calc_brt(ctx->mcu_info.clock_hz, ctx->comm_config.baud_transfer);
    uint8_t brt_csum = (2 * (256 - brt)) & 0xFF;
    uint8_t iap_wait = stc12_get_iap_delay(ctx->mcu_info.clock_hz);
    uint8_t delay = 0x80;
    
    /*========== 步骤1：发送握手请求 0x50 ==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_HANDSHAKE_REQ;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x36;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = (ctx->mcu_info.magic >> 8) & 0xFF;
    tx_buf[pos++] = ctx->mcu_info.magic & 0xFF;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x8F) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    /*========== 步骤2：测试新波特率 0x8F ==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_BAUD_TEST;
    tx_buf[pos++] = 0xC0;
    tx_buf[pos++] = brt;
    tx_buf[pos++] = 0x3F;
    tx_buf[pos++] = brt_csum;
    tx_buf[pos++] = delay;
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        /* 切回握手波特率重试 */
        ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x8F) {
        ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    /* 切回握手波特率 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
    
    /*========== 步骤3：切换到新波特率 0x8E ==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_BAUD_SWITCH;
    tx_buf[pos++] = 0xC0;
    tx_buf[pos++] = brt;
    tx_buf[pos++] = 0x3F;
    tx_buf[pos++] = brt_csum;
    tx_buf[pos++] = delay;
    
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
    
    if (rx_len < 1 || rx_buf[0] != 0x84) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 擦除Flash（带倒计时）
 *============================================================================*/
int stc12_erase_flash(stc_context_t* ctx, uint32_t size)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 计算块数（每512字节为1块，需要擦除2倍块数） */
    uint16_t blks = ((size + 511) / 512) * 2;
    uint16_t total_size = ((ctx->mcu_info.flash_size + 511) / 512) * 2;
    
    uint8_t tx_buf[128];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    /* 构建擦除数据包 */
    tx_buf[pos++] = STC_CMD_ERASE_84;
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = blks & 0xFF;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = total_size & 0xFF;
    
    /* 填充19字节的0x00 */
    for (int i = 0; i < 19; i++) {
        tx_buf[pos++] = 0x00;
    }
    
    /* 倒计时序列（从0x80到擦除倒计时结束值） */
    for (int i = 0x80; i >= ctx->config->erase_countdown; i--) {
        tx_buf[pos++] = i;
    }
    
    int ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 等待擦除完成（较长超时） */
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.erase_timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_ERASE_FAIL;
    }
    
    /* 验证响应 */
    if (rx_len < 1 || rx_buf[0] != 0x00) {
        return STC_ERR_ERASE_FAIL;
    }
    
    /* 提取UID（如果存在） */
    if (rx_len >= 8 && ctx->mcu_info.uid_valid == 0) {
        memcpy(ctx->mcu_info.uid, &rx_buf[1], STC_UID_SIZE);
        ctx->mcu_info.uid_valid = 1;
    }
    
    return STC_OK;
}

/*============================================================================
 * 编程单个数据块
 *============================================================================*/
int stc12_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, 
                        uint16_t len, uint8_t is_first)
{
    (void)is_first;  /* STC12不区分首块 */
    
    if (ctx == NULL || data == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[256];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    /* 3字节填充 */
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    
    /* 地址（大端序） */
    tx_buf[pos++] = (addr >> 8) & 0xFF;
    tx_buf[pos++] = addr & 0xFF;
    
    /* 块大小（大端序） */
    uint16_t block_size = ctx->config->block_size;
    tx_buf[pos++] = (block_size >> 8) & 0xFF;
    tx_buf[pos++] = block_size & 0xFF;
    
    /* 数据 */
    memcpy(&tx_buf[pos], data, len);
    pos += len;
    
    /* 填充到块大小 */
    while (pos < block_size + 7) {
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
    if (rx_len < 1 || rx_buf[0] != 0x00) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 编程完成
 *============================================================================*/
int stc12_program_finish(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    /* 发送完成命令 0x69 */
    tx_buf[pos++] = STC_CMD_FINISH;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x36;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = (ctx->mcu_info.magic >> 8) & 0xFF;
    tx_buf[pos++] = ctx->mcu_info.magic & 0xFF;
    
    int ret = send_packet(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x8D) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * 设置选项字节
 *============================================================================*/
int stc12_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    if (ctx == NULL || options == NULL || len < 4) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_SET_OPTIONS_8D;
    
    /* 添加4字节选项 */
    memcpy(&tx_buf[pos], options, 4);
    pos += 4;
    
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
int stc12_disconnect(stc_context_t* ctx)
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
const stc_protocol_ops_t stc12_protocol_ops = {
    .parse_status_packet = stc12_parse_status_packet,
    .handshake = stc12_handshake,
    .calibrate_frequency = NULL,    /* STC12不需要频率校准 */
    .erase_flash = stc12_erase_flash,
    .program_block = stc12_program_block,
    .program_finish = stc12_program_finish,
    .set_options = stc12_set_options,
    .disconnect = stc12_disconnect,
};

