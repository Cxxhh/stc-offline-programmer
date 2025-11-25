/**
  ******************************************************************************
  * @file    log.h
  * @brief   日志服务层头文件
  *          支持注册式输出接口，支持不同日志级别
  * @note    支持printf重定向和非printf格式两种注册方式
  * @version V2.0.0
  * @date    2025-01-XX
  ******************************************************************************
  */

#ifndef __LOG_H__
#define __LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

/* Configuration -------------------------------------------------------------*/
/**
 * @brief 条件编译选项
 */

/* 启用printf重定向支持（需要重定向printf到日志系统） */
#ifndef LOG_ENABLE_PRINTF_REDIRECT
#define LOG_ENABLE_PRINTF_REDIRECT  0
#endif

/* 启用时间戳输出 */
#ifndef LOG_ENABLE_TIMESTAMP
#define LOG_ENABLE_TIMESTAMP        1
#endif

/* 最大输出句柄数量 */
#ifndef LOG_MAX_OUTPUT_HANDLES
#define LOG_MAX_OUTPUT_HANDLES      4
#endif

/* 日志消息缓冲区大小（用于非printf格式输出） */
#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE             256
#endif

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 日志级别枚举
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,    /**< 调试信息（开发阶段） */
    LOG_LEVEL_INFO  = 1,    /**< 一般信息 */
    LOG_LEVEL_WARN  = 2,    /**< 警告信息 */
    LOG_LEVEL_ERROR = 3     /**< 错误信息 */
} log_level_t;

/**
 * @brief 日志输出类型枚举
 */
typedef enum {
    LOG_OUTPUT_TYPE_RAW = 0,    /**< 原始数据输出（需要先sprintf格式化） */
    LOG_OUTPUT_TYPE_PRINTF      /**< printf格式输出（直接格式化输出） */
} log_output_type_t;

/**
 * @brief 原始数据输出函数指针类型（非printf格式）
 * @param data 要输出的数据（已格式化的字符串）
 * @param len 数据长度
 * @return 成功返回0，失败返回非0
 */
typedef int (*log_output_raw_func_t)(const uint8_t *data, uint32_t len);

/**
 * @brief printf格式输出函数指针类型
 * @param format 格式化字符串（已包含级别前缀）
 * @param args 可变参数列表
 * @return 成功返回0，失败返回非0
 */
typedef int (*log_output_printf_func_t)(const char *format, va_list args);

/**
 * @brief 日志输出句柄结构
 */
typedef struct {
    log_output_type_t output_type;  /**< 输出类型 */
    union {
        log_output_raw_func_t raw_func;        /**< 原始数据输出函数指针 */
        log_output_printf_func_t printf_func;   /**< printf格式输出函数指针 */
    } output_func;
    void *user_data;                /**< 用户数据指针（可选） */
} log_output_handle_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief 初始化日志服务
 * @note  必须在使用其他API前调用
 */
void log_init(void);

/**
 * @brief 设置日志最低记录级别
 * @param min_level 最低级别（低于此级别的日志将被忽略）
 * @note  默认为LOG_LEVEL_DEBUG
 */
void log_set_level(log_level_t min_level);

/**
 * @brief 获取当前日志最低记录级别
 * @return 当前最低级别
 */
log_level_t log_get_level(void);

/**
 * @brief 注册日志输出接口（原始数据输出，需要先sprintf格式化）
 * @param handle 输出句柄指针，output_type应为LOG_OUTPUT_TYPE_RAW
 * @return 成功返回0，失败返回非0
 */
int log_register_output(log_output_handle_t *handle);

/**
 * @brief 注册日志输出接口（printf格式输出）
 * @param handle 输出句柄指针，output_type应为LOG_OUTPUT_TYPE_PRINTF
 * @return 成功返回0，失败返回非0
 */
int log_register_output_printf(log_output_handle_t *handle);

/**
 * @brief 注销日志输出接口
 * @param handle 输出句柄指针
 * @return 成功返回0，失败返回非0
 */
int log_unregister_output(log_output_handle_t *handle);

/**
 * @brief 写入日志（printf风格）
 * @param level 日志级别
 * @param format 格式化字符串（printf风格）
 * @param ... 可变参数
 * @note  日志格式：[时间戳] [级别] 消息
 */
void log_write(log_level_t level, const char *format, ...);

/* Exported macros -----------------------------------------------------------*/

/**
 * @brief 便捷日志宏（推荐使用）
 */
#define LOG_DEBUG(fmt, ...)  log_write(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   log_write(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   log_write(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  log_write(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/**
 * @brief 条件编译：Release模式禁用DEBUG日志
 */
#ifdef RELEASE_BUILD
    #undef LOG_DEBUG
    #define LOG_DEBUG(fmt, ...)  // 空宏，编译时删除
#endif

#if LOG_ENABLE_PRINTF_REDIRECT
/**
 * @brief printf重定向到日志系统（INFO级别）
 * @note  需要在syscalls.c或类似文件中重定向printf
 */
int log_printf_redirect(const char *format, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H__ */
