/**
 * @file stc_context.c
 * @brief STC烧录器上下文管理实现
 */

#include "stc_context.h"
#include <string.h>

/**
 * @brief 初始化上下文
 */
void stc_context_init(stc_context_t* ctx, const stc_hal_t* hal, void* uart_handle)
{
    if (ctx == NULL) {
        return;
    }
    
    /* 清零整个上下文 */
    memset(ctx, 0, sizeof(stc_context_t));
    
    /* 设置HAL接口 */
    ctx->hal = hal;
    ctx->uart_handle = uart_handle;
    
    /* 设置默认通信参数 */
    ctx->comm_config.baud_handshake = STC_DEFAULT_BAUD_HANDSHAKE;
    ctx->comm_config.baud_transfer = STC_DEFAULT_BAUD_TRANSFER;
    ctx->comm_config.timeout_ms = STC_DEFAULT_TIMEOUT_MS;
    ctx->comm_config.erase_timeout_ms = STC_ERASE_TIMEOUT_MS;
    
    /* 默认自动检测模式 */
    ctx->select_mode = STC_SELECT_AUTO;
}

/**
 * @brief 重置上下文（保留HAL配置）
 */
void stc_context_reset(stc_context_t* ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    /* 保存需要保留的字段 */
    const stc_hal_t* hal = ctx->hal;
    void* uart_handle = ctx->uart_handle;
    stc_comm_config_t comm_config = ctx->comm_config;
    stc_progress_cb_t progress_cb = ctx->progress_cb;
    void* progress_user_data = ctx->progress_user_data;
    stc_log_cb_t log_cb = ctx->log_cb;
    void* log_user_data = ctx->log_user_data;
    stc_select_mode_t select_mode = ctx->select_mode;
    stc_protocol_id_t manual_proto_id = ctx->manual_proto_id;
    
    /* 清零 */
    memset(ctx, 0, sizeof(stc_context_t));
    
    /* 恢复保留字段 */
    ctx->hal = hal;
    ctx->uart_handle = uart_handle;
    ctx->comm_config = comm_config;
    ctx->progress_cb = progress_cb;
    ctx->progress_user_data = progress_user_data;
    ctx->log_cb = log_cb;
    ctx->log_user_data = log_user_data;
    ctx->select_mode = select_mode;
    ctx->manual_proto_id = manual_proto_id;
}

/**
 * @brief 设置通信参数
 */
void stc_context_set_baudrate(stc_context_t* ctx, uint32_t baud_handshake, uint32_t baud_transfer)
{
    if (ctx == NULL) {
        return;
    }
    
    if (baud_handshake > 0) {
        ctx->comm_config.baud_handshake = baud_handshake;
    }
    if (baud_transfer > 0) {
        ctx->comm_config.baud_transfer = baud_transfer;
    }
}

/**
 * @brief 设置进度回调
 */
void stc_context_set_progress_callback(stc_context_t* ctx, stc_progress_cb_t cb, void* user_data)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->progress_cb = cb;
    ctx->progress_user_data = user_data;
}

/**
 * @brief 设置日志回调
 */
void stc_context_set_log_callback(stc_context_t* ctx, stc_log_cb_t cb, void* user_data)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->log_cb = cb;
    ctx->log_user_data = log_user_data;
}

