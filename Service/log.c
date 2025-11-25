/**
  ******************************************************************************
  * @file    log.c
  * @brief   日志服务层实现文件
  *          支持注册式输出接口，支持不同日志级别
  * @note    支持printf重定向和非printf格式两种注册方式
  * @version V2.0.0
  * @date    2025-01-XX
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "log.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
 * @brief 当前最低记录级别
 */
static log_level_t s_min_level = LOG_LEVEL_DEBUG;

/**
 * @brief 输出句柄数组
 */
static log_output_handle_t s_output_handles[LOG_MAX_OUTPUT_HANDLES];

/**
 * @brief 已注册的输出句柄数量
 */
static uint8_t s_output_count = 0;

/**
 * @brief 日志缓冲区（用于非printf格式输出）
 */
static char s_log_buffer[LOG_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/
static const char *log_get_level_string(log_level_t level);
static uint32_t log_get_tick(void);
static void log_format_message(log_level_t level, const char *format, va_list args, char *buffer, uint32_t buffer_size);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化日志服务
 */
void log_init(void)
{
    s_min_level = LOG_LEVEL_DEBUG;
    s_output_count = 0;
    memset(s_output_handles, 0, sizeof(s_output_handles));
}

/**
 * @brief 设置日志最低记录级别
 */
void log_set_level(log_level_t min_level)
{
    if (min_level <= LOG_LEVEL_ERROR)
    {
        s_min_level = min_level;
    }
}

/**
 * @brief 获取当前日志最低记录级别
 */
log_level_t log_get_level(void)
{
    return s_min_level;
}

/**
 * @brief 注册日志输出接口（原始数据输出，需要先sprintf格式化）
 */
int log_register_output(log_output_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }

    if (handle->output_type != LOG_OUTPUT_TYPE_RAW || handle->output_func.raw_func == NULL)
    {
        return -2;  // 类型错误或函数指针为空
    }

    if (s_output_count >= LOG_MAX_OUTPUT_HANDLES)
    {
        return -3;  // 输出句柄已满
    }

    // 检查是否已注册
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_RAW &&
            s_output_handles[i].output_func.raw_func == handle->output_func.raw_func)
        {
            return -4;  // 已注册
        }
    }

    // 注册新的输出句柄
    s_output_handles[s_output_count] = *handle;
    s_output_count++;

    return 0;
}

/**
 * @brief 注册日志输出接口（printf格式输出）
 */
int log_register_output_printf(log_output_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }

    if (handle->output_type != LOG_OUTPUT_TYPE_PRINTF || handle->output_func.printf_func == NULL)
    {
        return -2;  // 类型错误或函数指针为空
    }

    if (s_output_count >= LOG_MAX_OUTPUT_HANDLES)
    {
        return -3;  // 输出句柄已满
    }

    // 检查是否已注册
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_PRINTF &&
            s_output_handles[i].output_func.printf_func == handle->output_func.printf_func)
        {
            return -4;  // 已注册
        }
    }

    // 注册新的输出句柄
    s_output_handles[s_output_count] = *handle;
    s_output_count++;

    return 0;
}

/**
 * @brief 注销日志输出接口
 */
int log_unregister_output(log_output_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }

    // 查找并移除
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        int found = 0;
        if (handle->output_type == LOG_OUTPUT_TYPE_RAW)
        {
            if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_RAW &&
                s_output_handles[i].output_func.raw_func == handle->output_func.raw_func)
            {
                found = 1;
            }
        }
        else if (handle->output_type == LOG_OUTPUT_TYPE_PRINTF)
        {
            if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_PRINTF &&
                s_output_handles[i].output_func.printf_func == handle->output_func.printf_func)
            {
                found = 1;
            }
        }

        if (found)
        {
            // 将后面的元素前移
            for (uint8_t j = i; j < s_output_count - 1; j++)
            {
                s_output_handles[j] = s_output_handles[j + 1];
            }
            s_output_count--;
            memset(&s_output_handles[s_output_count], 0, sizeof(log_output_handle_t));
            return 0;
        }
    }

    return -2;  // 未找到
}

/**
 * @brief 写入日志（printf风格）
 */
void log_write(log_level_t level, const char *format, ...)
{
    // 级别过滤
    if (level < s_min_level || level > LOG_LEVEL_ERROR)
    {
        return;
    }

    // 参数检查
    if (format == NULL || s_output_count == 0)
    {
        return;
    }

    va_list args;
    va_start(args, format);

    // 处理原始数据输出接口（需要先sprintf格式化）
    int has_raw_output = 0;
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_RAW)
        {
            has_raw_output = 1;
            break;
        }
    }

    if (has_raw_output)
    {
        // 复制va_list（因为printf格式输出还需要使用）
        va_list args_copy;
        va_copy(args_copy, args);

        // 格式化日志消息
        log_format_message(level, format, args_copy, s_log_buffer, LOG_BUFFER_SIZE);

        va_end(args_copy);

        // 输出到所有原始数据输出接口
        for (uint8_t i = 0; i < s_output_count; i++)
        {
            if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_RAW &&
                s_output_handles[i].output_func.raw_func != NULL)
            {
                s_output_handles[i].output_func.raw_func((const uint8_t *)s_log_buffer, strlen(s_log_buffer));
            }
        }
    }

    // 处理printf格式输出接口
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_PRINTF &&
            s_output_handles[i].output_func.printf_func != NULL)
        {
            // 创建格式化字符串（包含级别前缀和时间戳）
            const char *level_str = log_get_level_string(level);
            char full_format[LOG_BUFFER_SIZE];
            
#if LOG_ENABLE_TIMESTAMP
            uint32_t timestamp = log_get_tick();
            snprintf(full_format, LOG_BUFFER_SIZE, "[%08lu] %s %s\r\n", timestamp, level_str, format);
#else
            snprintf(full_format, LOG_BUFFER_SIZE, "%s %s\r\n", level_str, format);
#endif

            // 复制va_list（因为va_list只能使用一次）
            va_list args_copy;
            va_copy(args_copy, args);

            // 使用printf格式输出
            s_output_handles[i].output_func.printf_func(full_format, args_copy);

            va_end(args_copy);
        }
    }

    va_end(args);
}

#if LOG_ENABLE_PRINTF_REDIRECT
/**
 * @brief printf重定向到日志系统（INFO级别）
 */
int log_printf_redirect(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    // 创建格式化字符串
    char full_format[LOG_BUFFER_SIZE];
#if LOG_ENABLE_TIMESTAMP
    uint32_t timestamp = log_get_tick();
    snprintf(full_format, LOG_BUFFER_SIZE, "[%08lu] [INFO] %s\r\n", timestamp, format);
#else
    snprintf(full_format, LOG_BUFFER_SIZE, "[INFO] %s\r\n", format);
#endif

    // 输出到所有printf格式输出接口
    for (uint8_t i = 0; i < s_output_count; i++)
    {
        if (s_output_handles[i].output_type == LOG_OUTPUT_TYPE_PRINTF &&
            s_output_handles[i].output_func.printf_func != NULL)
        {
            va_list args_copy;
            va_copy(args_copy, args);
            s_output_handles[i].output_func.printf_func(full_format, args_copy);
            va_end(args_copy);
        }
    }

    va_end(args);
    return 0;
}
#endif

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 获取日志级别字符串
 */
static const char *log_get_level_string(log_level_t level)
{
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        return "[DEBUG]";
    case LOG_LEVEL_INFO:
        return "[INFO]";
    case LOG_LEVEL_WARN:
        return "[WARN]";
    case LOG_LEVEL_ERROR:
        return "[ERROR]";
    default:
        return "[UNKN]";
    }
}

/**
 * @brief 获取系统时钟滴答（毫秒）
 */
static uint32_t log_get_tick(void)
{
    extern uint32_t HAL_GetTick(void);
    return HAL_GetTick();
}

/**
 * @brief 格式化日志消息（用于原始数据输出）
 */
static void log_format_message(log_level_t level, const char *format, va_list args, char *buffer, uint32_t buffer_size)
{
    const char *level_str = log_get_level_string(level);
    int pos = 0;

#if LOG_ENABLE_TIMESTAMP
    // 添加时间戳和级别前缀
    uint32_t timestamp = log_get_tick();
    pos = snprintf(buffer, buffer_size, "[%08lu] %s ", timestamp, level_str);
#else
    // 添加级别前缀
    pos = snprintf(buffer, buffer_size, "%s ", level_str);
#endif

    if (pos < 0 || pos >= (int)buffer_size)
    {
        return;
    }

    // 格式化用户消息
    int remaining = buffer_size - pos;
    int written = vsnprintf(buffer + pos, remaining, format, args);
    if (written < 0 || written >= remaining)
    {
        // 缓冲区不足，截断
        buffer[buffer_size - 1] = '\0';
        return;
    }

    // 添加换行符
    pos += written;
    if (pos < (int)(buffer_size - 2))
    {
        buffer[pos] = '\r';
        buffer[pos + 1] = '\n';
        buffer[pos + 2] = '\0';
    }
    else
    {
        buffer[buffer_size - 1] = '\0';
    }
}

