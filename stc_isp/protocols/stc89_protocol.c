/**
 * @file stc89_protocol.c
 * @brief STC89/89A系列协议实现
 */

#include "stc89_protocol.h"
#include "../stc_packet.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 内部函数声明
 *============================================================================*/
static int send_packet_89(stc_context_t* ctx, const uint8_t* payload, uint16_t len);
static int recv_packet_89(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms);
static int send_packet_89a(stc_context_t* ctx, const uint8_t* payload, uint16_t len);
static int recv_packet_89a(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms);

/*============================================================================
 * BRT和IAP计算
 *============================================================================*/
uint16_t stc89_calc_brt(float mcu_clock_hz, uint32_t baud_transfer, uint8_t cpu_6t)
{
    /* STC89: BRT = 65536 - clock / (baud * sample_rate) */
    /* sample_rate: 6T模式为16，12T模式为32 */
    uint8_t sample_rate = cpu_6t ? 16 : 32;
    
    int32_t brt = 65536 - (int32_t)(mcu_clock_hz / (baud_transfer * sample_rate) + 0.5f);
    
    if (brt < 0) brt = 0;
    if (brt > 65535) brt = 65535;
    
    return (uint16_t)brt;
}

uint8_t stc89_get_iap_delay(float clock_hz)
{
    if (clock_hz < 5E6)  return 0x83;
    if (clock_hz < 10E6) return 0x82;
    if (clock_hz < 20E6) return 0x81;
    return 0x80;
}

/*============================================================================
 * STC89数据包处理（单字节校验和）
 *============================================================================*/
static int send_packet_89(stc_context_t* ctx, const uint8_t* payload, uint16_t len)
{
    if (ctx == NULL || ctx->hal == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* STC89使用单字节校验和 */
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

static int recv_packet_89(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms)
{
    if (ctx == NULL || ctx->hal == NULL) {
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
 * STC89A数据包处理（双字节校验和）
 *============================================================================*/
static int send_packet_89a(stc_context_t* ctx, const uint8_t* payload, uint16_t len)
{
    return send_packet_89(ctx, payload, len);  /* 使用配置中的校验和类型 */
}

static int recv_packet_89a(stc_context_t* ctx, uint8_t* payload, uint16_t* len, uint32_t timeout_ms)
{
    return recv_packet_89(ctx, payload, len, timeout_ms);
}

/*============================================================================
 * STC89状态包解析
 *============================================================================*/
int stc89_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len)
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
    
    /* 检查CPU模式（6T或12T） */
    if (len >= 20) {
        ctx->mcu_info.cpu_6t = !(data[19] & 1);
    }
    
    /* 解析频率计数器（8次测量的平均值） */
    uint32_t freq_sum = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t count = (data[1 + 2*i] << 8) | data[2 + 2*i];
        freq_sum += count;
    }
    ctx->mcu_info.freq_counter = freq_sum / 8;
    
    /* 计算MCU频率 */
    /* STC89频率 = 波特率 * 频率计数 * CPU_T / 7 */
    float cpu_t = ctx->mcu_info.cpu_6t ? 6.0f : 12.0f;
    ctx->mcu_info.clock_hz = (float)ctx->comm_config.baud_handshake * 
                              ctx->mcu_info.freq_counter * cpu_t / 7.0f;
    
    return STC_OK;
}

/*============================================================================
 * STC89握手流程
 *============================================================================*/
int stc89_handshake(stc_context_t* ctx)
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
    uint16_t brt = stc89_calc_brt(ctx->mcu_info.clock_hz, ctx->comm_config.baud_transfer, ctx->mcu_info.cpu_6t);
    uint8_t brt_csum = (2 * (256 - (brt >> 8))) & 0xFF;
    uint8_t iap_wait = stc89_get_iap_delay(ctx->mcu_info.clock_hz);
    uint8_t delay = 0xA0;
    
    /*========== 步骤1：测试新波特率 0x8F ==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_BAUD_TEST;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = 0xFF - (brt >> 8);
    tx_buf[pos++] = brt_csum;
    tx_buf[pos++] = delay;
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet_89(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x8F) {
        ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    /* 切回握手波特率 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
    
    /*========== 步骤2：切换到新波特率 0x8E ==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_BAUD_SWITCH;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = 0xFF - (brt >> 8);
    tx_buf[pos++] = brt_csum;
    tx_buf[pos++] = delay;
    
    ret = send_packet_89(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(100);
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    
    ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x8E) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    /*========== 步骤3：Ping-Pong测试（4次）==========*/
    pos = 0;
    tx_buf[pos++] = STC_CMD_PING;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x36;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = (ctx->mcu_info.magic >> 8) & 0xFF;
    tx_buf[pos++] = ctx->mcu_info.magic & 0xFF;
    
    for (int i = 0; i < 4; i++) {
        ret = send_packet_89(ctx, tx_buf, pos);
        if (ret != STC_OK) {
            return ret;
        }
        
        ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
        if (ret != STC_OK) {
            return ret;
        }
        
        if (rx_len < 1 || rx_buf[0] != STC_CMD_PING) {
            return STC_ERR_HANDSHAKE_FAIL;
        }
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89擦除Flash
 *============================================================================*/
int stc89_erase_flash(stc_context_t* ctx, uint32_t size)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 计算块数（每512字节为1块，需要擦除2倍块数） */
    uint16_t blks = ((size + 511) / 512) * 2;
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_ERASE_84;
    tx_buf[pos++] = blks & 0xFF;
    tx_buf[pos++] = 0x33;
    tx_buf[pos++] = 0x33;
    tx_buf[pos++] = 0x33;
    tx_buf[pos++] = 0x33;
    tx_buf[pos++] = 0x33;
    tx_buf[pos++] = 0x33;
    
    int ret = send_packet_89(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 等待擦除完成 */
    ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.erase_timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_ERASE_FAIL;
    }
    
    if (rx_len < 1 || rx_buf[0] != STC_CMD_PING) {
        return STC_ERR_ERASE_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89编程单个数据块
 *============================================================================*/
int stc89_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, 
                        uint16_t len, uint8_t is_first)
{
    (void)is_first;
    
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
    
    /* 计算数据校验和 */
    uint8_t data_csum = 0;
    for (uint16_t i = 0; i < len; i++) {
        data_csum += data[i];
    }
    
    /* 填充到块大小 */
    while (pos < block_size + 7) {
        tx_buf[pos++] = 0x00;
    }
    
    int ret = send_packet_89(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    /* 验证响应 */
    if (rx_len < 1 || rx_buf[0] != STC_CMD_PING) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    /* 验证数据校验和 */
    if (rx_len >= 2 && rx_buf[1] != data_csum) {
        return STC_ERR_VERIFY_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89设置选项字节
 *============================================================================*/
int stc89_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    if (ctx == NULL || options == NULL || len < 1) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[8];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_SET_OPTIONS_8D;
    tx_buf[pos++] = options[0];     /* MSR */
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0xFF;
    tx_buf[pos++] = 0xFF;
    
    int ret = send_packet_89(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != STC_CMD_SET_OPTIONS_8D) {
        return STC_ERR_PROTOCOL;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89断开连接
 *============================================================================*/
int stc89_disconnect(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[4];
    tx_buf[0] = STC_CMD_DISCONNECT;
    
    send_packet_89(ctx, tx_buf, 1);
    
    return STC_OK;
}

/*============================================================================
 * STC89A状态包解析
 *============================================================================*/
int stc89a_parse_status_packet(stc_context_t* ctx, const uint8_t* data, uint16_t len)
{
    if (ctx == NULL || data == NULL || len < 15) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 保存原始状态包 */
    memcpy(ctx->status_packet, data, MIN(len, sizeof(ctx->status_packet)));
    ctx->status_packet_len = len;
    
    /* STC89A状态包格式不同 */
    /* 频率计数在偏移13-14 */
    ctx->mcu_info.freq_counter = (data[13] << 8) | data[14];
    
    /* 计算MCU频率 */
    ctx->mcu_info.clock_hz = 12.0f * ctx->mcu_info.freq_counter * ctx->comm_config.baud_handshake;
    
    /* Magic值位置 */
    if (len >= 22) {
        ctx->mcu_info.magic = (data[20] << 8) | data[21];
    }
    
    /* 默认12T模式 */
    ctx->mcu_info.cpu_6t = 0;
    
    return STC_OK;
}

/*============================================================================
 * STC89A握手流程
 *============================================================================*/
int stc89a_handshake(stc_context_t* ctx)
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
    uint8_t sample_rate = 32;
    uint16_t brt = 65536 - (uint16_t)(ctx->mcu_info.clock_hz / (ctx->comm_config.baud_transfer * sample_rate) + 0.5f);
    
    uint8_t iap_wait = 0x80;
    if (ctx->mcu_info.clock_hz < 10E6) iap_wait = 0x83;
    else if (ctx->mcu_info.clock_hz < 30E6) iap_wait = 0x82;
    else if (ctx->mcu_info.clock_hz < 50E6) iap_wait = 0x81;
    
    /*========== 测试新波特率 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x01;
    tx_buf[pos++] = (brt >> 8) & 0xFF;
    tx_buf[pos++] = brt & 0xFF;
    tx_buf[pos++] = iap_wait;
    
    ret = send_packet_89a(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ctx->hal->delay_ms(200);
    
    ret = recv_packet_89a(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x01) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    /* 切换波特率和校验位 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_transfer);
    ctx->hal->set_parity(ctx->uart_handle, STC_PARITY_EVEN);
    
    /*========== Ping-Pong测试 ==========*/
    pos = 0;
    tx_buf[pos++] = 0x05;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x46;
    tx_buf[pos++] = 0xB9;
    
    ret = send_packet_89a(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89a(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    if (rx_len < 1 || rx_buf[0] != 0x05) {
        return STC_ERR_HANDSHAKE_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89A擦除Flash（全擦除）
 *============================================================================*/
int stc89a_erase_flash(stc_context_t* ctx, uint32_t size)
{
    (void)size;  /* STC89A全擦除 */
    
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[8];
    uint8_t rx_buf[64];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_ERASE;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x00;
    tx_buf[pos++] = 0x46;
    tx_buf[pos++] = 0xB9;
    
    int ret = send_packet_89a(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89a(ctx, rx_buf, &rx_len, ctx->comm_config.erase_timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_ERASE_FAIL;
    }
    
    if (rx_len < 1 || rx_buf[0] != STC_CMD_ERASE) {
        return STC_ERR_ERASE_FAIL;
    }
    
    /* 提取MCU ID（7字节） */
    if (rx_len >= 8 && ctx->mcu_info.uid_valid == 0) {
        memcpy(ctx->mcu_info.uid, &rx_buf[1], STC_UID_SIZE);
        ctx->mcu_info.uid_valid = 1;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89A编程单个数据块
 *============================================================================*/
int stc89a_program_block(stc_context_t* ctx, uint32_t addr, const uint8_t* data, 
                         uint16_t len, uint8_t is_first)
{
    if (ctx == NULL || data == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[256];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    if (is_first) {
        /* 首块 */
        tx_buf[pos++] = STC_CMD_WRITE_FIRST;    /* 0x22 */
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x00;
    } else {
        /* 后续块 */
        tx_buf[pos++] = STC_CMD_WRITE_BLOCK;    /* 0x02 */
        tx_buf[pos++] = (addr >> 8) & 0xFF;
        tx_buf[pos++] = addr & 0xFF;
    }
    
    /* 魔术字 */
    tx_buf[pos++] = 0x46;
    tx_buf[pos++] = 0xB9;
    
    /* 数据 */
    memcpy(&tx_buf[pos], data, len);
    pos += len;
    
    int ret = send_packet_89a(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89a(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    if (rx_len < 1 || rx_buf[0] != STC_CMD_WRITE_BLOCK) {
        return STC_ERR_PROGRAM_FAIL;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89A设置选项字节
 *============================================================================*/
int stc89a_set_options(stc_context_t* ctx, const uint8_t* options, uint8_t len)
{
    if (ctx == NULL || options == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[16];
    uint8_t rx_buf[32];
    uint16_t rx_len;
    uint16_t pos = 0;
    
    tx_buf[pos++] = STC_CMD_SET_OPTIONS;
    
    /* 添加选项字节 */
    uint8_t opt_len = MIN(len, 4);
    memcpy(&tx_buf[pos], options, opt_len);
    pos += opt_len;
    
    /* 填充到4字节 */
    while (pos < 5) {
        tx_buf[pos++] = 0xFF;
    }
    
    int ret = send_packet_89a(ctx, tx_buf, pos);
    if (ret != STC_OK) {
        return ret;
    }
    
    ret = recv_packet_89a(ctx, rx_buf, &rx_len, ctx->comm_config.timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    return STC_OK;
}

/*============================================================================
 * STC89A断开连接
 *============================================================================*/
int stc89a_disconnect(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    uint8_t tx_buf[4];
    tx_buf[0] = STC_CMD_DISCONNECT_FF;
    
    send_packet_89a(ctx, tx_buf, 1);
    
    return STC_OK;
}

/*============================================================================
 * 协议操作表
 *============================================================================*/

/* STC89协议 */
const stc_protocol_ops_t stc89_protocol_ops = {
    .parse_status_packet = stc89_parse_status_packet,
    .handshake = stc89_handshake,
    .calibrate_frequency = NULL,    /* STC89不需要频率校准 */
    .erase_flash = stc89_erase_flash,
    .program_block = stc89_program_block,
    .program_finish = NULL,         /* STC89不需要完成命令 */
    .set_options = stc89_set_options,
    .disconnect = stc89_disconnect,
};

/* STC89A协议 */
const stc_protocol_ops_t stc89a_protocol_ops = {
    .parse_status_packet = stc89a_parse_status_packet,
    .handshake = stc89a_handshake,
    .calibrate_frequency = NULL,
    .erase_flash = stc89a_erase_flash,
    .program_block = stc89a_program_block,
    .program_finish = NULL,
    .set_options = stc89a_set_options,
    .disconnect = stc89a_disconnect,
};

