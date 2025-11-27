/**
 * @file stc_programmer.c
 * @brief STC烧录器主控流程实现
 */

#include "stc_programmer.h"
#include "stc_packet.h"
#include <string.h>

/*============================================================================
 * 错误信息表
 *============================================================================*/
static const char* g_error_strings[] = {
    "成功",                          // STC_OK
    "超时",                          // STC_ERR_TIMEOUT
    "校验和错误",                    // STC_ERR_CHECKSUM
    "帧格式错误",                    // STC_ERR_FRAME
    "协议错误",                      // STC_ERR_PROTOCOL
    "未知型号",                      // STC_ERR_UNKNOWN_MODEL
    "擦除失败",                      // STC_ERR_ERASE_FAIL
    "编程失败",                      // STC_ERR_PROGRAM_FAIL
    "验证失败",                      // STC_ERR_VERIFY_FAIL
    "握手失败",                      // STC_ERR_HANDSHAKE_FAIL
    "校准失败",                      // STC_ERR_CALIBRATION_FAIL
    "参数无效",                      // STC_ERR_INVALID_PARAM
    "无响应",                        // STC_ERR_NO_RESPONSE
    "MCU已锁定",                     // STC_ERR_MCU_LOCKED
};

/*============================================================================
 * 内部函数声明
 *============================================================================*/
static int wait_for_status_packet(stc_context_t* ctx, uint32_t timeout_ms);
static int parse_status_and_identify(stc_context_t* ctx);
static void update_progress(stc_context_t* ctx, uint32_t current, uint32_t total);

/*============================================================================
 * API实现
 *============================================================================*/

int stc_programmer_init(stc_context_t* ctx, const stc_hal_t* hal, void* uart_handle)
{
    if (ctx == NULL || hal == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    stc_context_init(ctx, hal, uart_handle);
    return STC_OK;
}

int stc_set_mode_auto(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    ctx->select_mode = STC_SELECT_AUTO;
    return STC_OK;
}

int stc_set_mode_manual(stc_context_t* ctx, stc_protocol_id_t proto_id)
{
    if (ctx == NULL || proto_id >= STC_PROTO_COUNT) {
        return STC_ERR_INVALID_PARAM;
    }
    
    ctx->select_mode = STC_SELECT_MANUAL;
    ctx->manual_proto_id = proto_id;
    
    /* 手动模式下预先加载协议 */
    return stc_get_protocol_by_id(proto_id, &ctx->config, &ctx->ops);
}

int stc_connect(stc_context_t* ctx, uint32_t timeout_ms)
{
    if (ctx == NULL || ctx->hal == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 重置上下文 */
    stc_context_reset(ctx);
    
    /* 设置握手波特率 */
    ctx->hal->set_baudrate(ctx->uart_handle, ctx->comm_config.baud_handshake);
    
    /* 初始无校验位 */
    ctx->hal->set_parity(ctx->uart_handle, STC_PARITY_NONE);
    
    /* 清空缓冲区 */
    ctx->hal->flush(ctx->uart_handle);
    
    /* 等待状态包 */
    int ret = wait_for_status_packet(ctx, timeout_ms);
    if (ret != STC_OK) {
        return ret;
    }
    
    /* 解析状态包并识别MCU */
    ret = parse_status_and_identify(ctx);
    if (ret != STC_OK) {
        return ret;
    }
    
    return STC_OK;
}

int stc_select_protocol(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    if (ctx->select_mode == STC_SELECT_MANUAL) {
        /* 手动模式：使用用户指定的协议 */
        int ret = stc_get_protocol_by_id(ctx->manual_proto_id, &ctx->config, &ctx->ops);
        if (ret == STC_OK) {
            ctx->proto_detected = 1;
            ctx->detected_proto_id = ctx->manual_proto_id;
        }
        return ret;
    }
    
    /* 自动模式：根据型号名称匹配协议 */
    if (ctx->mcu_info.model_name == NULL) {
        return STC_ERR_UNKNOWN_MODEL;
    }
    
    int ret = stc_match_protocol_by_name(ctx->mcu_info.model_name,
                                          &ctx->config, &ctx->ops,
                                          &ctx->detected_proto_id);
    if (ret == STC_OK) {
        ctx->proto_detected = 1;
    }
    
    return ret;
}

int stc_program(stc_context_t* ctx, const uint8_t* data, uint32_t len, 
                const stc_program_config_t* config)
{
    if (ctx == NULL || data == NULL || len == 0) {
        return STC_ERR_INVALID_PARAM;
    }
    
    if (ctx->config == NULL || ctx->ops == NULL) {
        return STC_ERR_PROTOCOL;
    }
    
    int ret;
    
    /* 应用配置 */
    if (config != NULL) {
        if (config->baud_handshake > 0) {
            ctx->comm_config.baud_handshake = config->baud_handshake;
        }
        if (config->baud_transfer > 0) {
            ctx->comm_config.baud_transfer = config->baud_transfer;
        }
    }
    
    /*========== 1. 握手/波特率切换 ==========*/
    if (ctx->ops->handshake != NULL) {
        ret = ctx->ops->handshake(ctx);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /*========== 2. 频率校准（如果需要）==========*/
    if (ctx->config->needs_freq_calib && ctx->ops->calibrate_frequency != NULL) {
        float target_freq = (config != NULL) ? config->target_frequency : 0;
        ret = ctx->ops->calibrate_frequency(ctx, target_freq);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /*========== 3. 擦除Flash ==========*/
    if (ctx->ops->erase_flash != NULL) {
        ret = ctx->ops->erase_flash(ctx, len);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /*========== 4. 分块编程 ==========*/
    if (ctx->ops->program_block != NULL) {
        uint32_t addr = 0;
        uint16_t block_size = ctx->config->block_size;
        uint8_t is_first = 1;
        
        while (addr < len) {
            uint16_t block_len = (len - addr < block_size) ? (len - addr) : block_size;
            
            ret = ctx->ops->program_block(ctx, addr, &data[addr], block_len, is_first);
            if (ret != STC_OK) {
                return ret;
            }
            
            addr += block_len;
            is_first = 0;
            
            /* 更新进度 */
            update_progress(ctx, addr, len);
        }
    }
    
    /*========== 5. 编程完成确认 ==========*/
    if (ctx->ops->program_finish != NULL) {
        ret = ctx->ops->program_finish(ctx);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /*========== 6. 设置选项字节（可选）==========*/
    /* 如果需要设置选项字节，在这里添加 */
    
    /*========== 7. 断开连接 ==========*/
    if (ctx->ops->disconnect != NULL) {
        ctx->ops->disconnect(ctx);
    }
    
    return STC_OK;
}

int stc_erase_only(stc_context_t* ctx, uint8_t erase_eeprom)
{
    (void)erase_eeprom;  /* TODO: 支持EEPROM擦除选项 */
    
    if (ctx == NULL || ctx->ops == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    int ret;
    
    /* 握手 */
    if (ctx->ops->handshake != NULL) {
        ret = ctx->ops->handshake(ctx);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /* 频率校准 */
    if (ctx->config->needs_freq_calib && ctx->ops->calibrate_frequency != NULL) {
        ret = ctx->ops->calibrate_frequency(ctx, 0);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /* 擦除 */
    if (ctx->ops->erase_flash != NULL) {
        ret = ctx->ops->erase_flash(ctx, ctx->mcu_info.flash_size);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    /* 断开 */
    if (ctx->ops->disconnect != NULL) {
        ctx->ops->disconnect(ctx);
    }
    
    return STC_OK;
}

int stc_disconnect(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    if (ctx->ops != NULL && ctx->ops->disconnect != NULL) {
        ctx->ops->disconnect(ctx);
    }
    
    return STC_OK;
}

const stc_mcu_info_t* stc_get_mcu_info(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return NULL;
    }
    return &ctx->mcu_info;
}

stc_protocol_id_t stc_get_detected_protocol(stc_context_t* ctx)
{
    if (ctx == NULL || !ctx->proto_detected) {
        return STC_PROTO_COUNT;
    }
    return ctx->detected_proto_id;
}

const char* stc_get_error_string(int error)
{
    if (error >= 0) {
        return g_error_strings[0];
    }
    
    int index = -error;
    if (index >= (int)(sizeof(g_error_strings) / sizeof(g_error_strings[0]))) {
        return "未知错误";
    }
    
    return g_error_strings[index];
}

/*============================================================================
 * 内部函数实现
 *============================================================================*/

static int wait_for_status_packet(stc_context_t* ctx, uint32_t timeout_ms)
{
    uint32_t start_tick = ctx->hal->get_tick_ms();
    uint8_t sync_char = STC_SYNC_CHAR;
    
    /* 发送同步字符直到收到响应 */
    while (1) {
        /* 发送同步字符 */
        ctx->hal->write(ctx->uart_handle, &sync_char, 1, 100);
        ctx->hal->delay_ms(30);
        
        /* 尝试读取状态包 */
        int rx_len = ctx->hal->read(ctx->uart_handle, ctx->rx_buffer, 
                                     sizeof(ctx->rx_buffer), 100);
        
        if (rx_len > 0) {
            /* 检查是否为有效的状态包 */
            if (rx_len >= 20 && 
                ctx->rx_buffer[0] == STC_FRAME_START1 && 
                ctx->rx_buffer[1] == STC_FRAME_START2) {
                ctx->rx_len = rx_len;
                return STC_OK;
            }
        }
        
        /* 检查超时 */
        if (timeout_ms > 0) {
            uint32_t elapsed = ctx->hal->get_tick_ms() - start_tick;
            if (elapsed >= timeout_ms) {
                return STC_ERR_TIMEOUT;
            }
        }
    }
}

static int parse_status_and_identify(stc_context_t* ctx)
{
    /* 首先尝试使用双字节校验和解析（大多数型号） */
    stc_packet_info_t info;
    
    /* 尝试用默认配置解析 */
    int ret = stc_parse_packet(&stc_config_stc15, ctx->rx_buffer, ctx->rx_len, &info);
    
    if (ret != STC_OK) {
        /* 尝试用单字节校验和解析（STC89） */
        ret = stc_parse_packet(&stc_config_stc89, ctx->rx_buffer, ctx->rx_len, &info);
        if (ret != STC_OK) {
            return STC_ERR_FRAME;
        }
    }
    
    /* 提取Magic值 */
    if (info.payload_len >= 22) {
        ctx->mcu_info.magic = (info.payload[20] << 8) | info.payload[21];
    } else if (info.payload_len >= 17) {
        /* 某些型号Magic值在不同位置 */
        ctx->mcu_info.magic = (info.payload[15] << 8) | info.payload[16];
    }
    
    /* 查找型号 */
    const stc_model_info_t* model = stc_find_model_by_magic(ctx->mcu_info.magic);
    if (model != NULL) {
        ctx->mcu_info.model_name = model->name;
        ctx->mcu_info.flash_size = model->flash_size;
        ctx->mcu_info.eeprom_size = model->eeprom_size;
        
        /* 自动模式下根据型号选择协议 */
        if (ctx->select_mode == STC_SELECT_AUTO) {
            stc_get_protocol_by_id(model->protocol_id, &ctx->config, &ctx->ops);
            ctx->detected_proto_id = model->protocol_id;
            ctx->proto_detected = 1;
        }
    } else {
        /* 未知型号 */
        if (ctx->select_mode == STC_SELECT_AUTO) {
            return STC_ERR_UNKNOWN_MODEL;
        }
    }
    
    /* 使用选定的协议解析状态包 */
    if (ctx->ops != NULL && ctx->ops->parse_status_packet != NULL) {
        ret = ctx->ops->parse_status_packet(ctx, info.payload, info.payload_len);
        if (ret != STC_OK) {
            return ret;
        }
    }
    
    return STC_OK;
}

static void update_progress(stc_context_t* ctx, uint32_t current, uint32_t total)
{
    if (ctx->progress_cb != NULL) {
        ctx->progress_cb(current, total, ctx->progress_user_data);
    }
}

