/**
  ******************************************************************************
  * @file    ringbuffer.c
  * @brief   环形缓冲区服务层实现文件
  *          支持动态注册，注册时指定缓冲区大小
  * @version V1.0.0
  * @date    2025-12-01
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ringbuffer.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/**
 * @brief 环形缓冲区实例池
 */
static ringbuffer_handle_t s_ringbuffer_pool[RINGBUFFER_MAX_INSTANCES];

/**
 * @brief 初始化标志
 */
static bool s_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static bool ringbuffer_is_valid(ringbuffer_handle_t *handle);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化环形缓冲区服务
 */
void ringbuffer_init(void)
{
    memset(s_ringbuffer_pool, 0, sizeof(s_ringbuffer_pool));
    s_initialized = true;
}

/**
 * @brief 注册一个环形缓冲区实例
 */
ringbuffer_handle_t *ringbuffer_register(uint8_t *buffer, uint16_t size)
{
    // 参数检查
    if (buffer == NULL || size < 2)
    {
        return NULL;
    }

    // 确保已初始化
    if (!s_initialized)
    {
        ringbuffer_init();
    }

    // 查找空闲实例
    for (uint8_t i = 0; i < RINGBUFFER_MAX_INSTANCES; i++)
    {
        if (!s_ringbuffer_pool[i].is_registered)
        {
            // 初始化实例
            s_ringbuffer_pool[i].buffer = buffer;
            s_ringbuffer_pool[i].size = size;
            s_ringbuffer_pool[i].head = 0;
            s_ringbuffer_pool[i].tail = 0;
            s_ringbuffer_pool[i].is_registered = true;

            return &s_ringbuffer_pool[i];
        }
    }

    // 无可用实例
    return NULL;
}

/**
 * @brief 注销环形缓冲区实例
 */
ringbuffer_status_t ringbuffer_unregister(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    // 清除实例
    handle->buffer = NULL;
    handle->size = 0;
    handle->head = 0;
    handle->tail = 0;
    handle->is_registered = false;

    return RINGBUFFER_OK;
}

/**
 * @brief 写入单个字节
 */
ringbuffer_status_t ringbuffer_write_byte(ringbuffer_handle_t *handle, uint8_t data)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    uint16_t next_head = (handle->head + 1) % handle->size;

    // 检查是否已满
    if (next_head == handle->tail)
    {
        return RINGBUFFER_FULL;
    }

    // 写入数据
    handle->buffer[handle->head] = data;
    handle->head = next_head;

    return RINGBUFFER_OK;
}

/**
 * @brief 写入多个字节
 */
uint16_t ringbuffer_write(ringbuffer_handle_t *handle, const uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(handle) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t written = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (ringbuffer_write_byte(handle, data[i]) != RINGBUFFER_OK)
        {
            break;
        }
        written++;
    }

    return written;
}

/**
 * @brief 读取单个字节
 */
ringbuffer_status_t ringbuffer_read_byte(ringbuffer_handle_t *handle, uint8_t *data)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    if (data == NULL)
    {
        return RINGBUFFER_ERROR;
    }

    // 检查是否为空
    if (handle->head == handle->tail)
    {
        return RINGBUFFER_EMPTY;
    }

    // 读取数据
    *data = handle->buffer[handle->tail];
    handle->tail = (handle->tail + 1) % handle->size;

    return RINGBUFFER_OK;
}

/**
 * @brief 读取多个字节
 */
uint16_t ringbuffer_read(ringbuffer_handle_t *handle, uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(handle) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t read_count = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (ringbuffer_read_byte(handle, &data[i]) != RINGBUFFER_OK)
        {
            break;
        }
        read_count++;
    }

    return read_count;
}

/**
 * @brief 查看单个字节（不移除）
 */
ringbuffer_status_t ringbuffer_peek(ringbuffer_handle_t *handle, uint8_t *data)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    if (data == NULL)
    {
        return RINGBUFFER_ERROR;
    }

    // 检查是否为空
    if (handle->head == handle->tail)
    {
        return RINGBUFFER_EMPTY;
    }

    // 读取数据但不移动tail
    *data = handle->buffer[handle->tail];

    return RINGBUFFER_OK;
}

/**
 * @brief 查看多个字节（不移除）
 */
uint16_t ringbuffer_peek_multiple(ringbuffer_handle_t *handle, uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(handle) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t count = ringbuffer_get_count(handle);
    uint16_t peek_len = (len < count) ? len : count;
    uint16_t temp_tail = handle->tail;

    for (uint16_t i = 0; i < peek_len; i++)
    {
        data[i] = handle->buffer[temp_tail];
        temp_tail = (temp_tail + 1) % handle->size;
    }

    return peek_len;
}

/**
 * @brief 丢弃指定数量的字节
 */
uint16_t ringbuffer_discard(ringbuffer_handle_t *handle, uint16_t len)
{
    if (!ringbuffer_is_valid(handle) || len == 0)
    {
        return 0;
    }

    uint16_t count = ringbuffer_get_count(handle);
    uint16_t discard_len = (len < count) ? len : count;

    handle->tail = (handle->tail + discard_len) % handle->size;

    return discard_len;
}

/**
 * @brief 获取缓冲区中可读数据量
 */
uint16_t ringbuffer_get_count(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return 0;
    }

    if (handle->head >= handle->tail)
    {
        return handle->head - handle->tail;
    }
    else
    {
        return handle->size - handle->tail + handle->head;
    }
}

/**
 * @brief 获取缓冲区剩余空间
 */
uint16_t ringbuffer_get_free(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return 0;
    }

    // 环形缓冲区实际可用空间为 size - 1（需要保留一个位置区分满和空）
    return (handle->size - 1) - ringbuffer_get_count(handle);
}

/**
 * @brief 获取缓冲区总大小
 */
uint16_t ringbuffer_get_size(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return 0;
    }

    return handle->size;
}

/**
 * @brief 检查缓冲区是否为空
 */
bool ringbuffer_is_empty(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return true;
    }

    return (handle->head == handle->tail);
}

/**
 * @brief 检查缓冲区是否已满
 */
bool ringbuffer_is_full(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return true;
    }

    uint16_t next_head = (handle->head + 1) % handle->size;
    return (next_head == handle->tail);
}

/**
 * @brief 清空缓冲区
 */
ringbuffer_status_t ringbuffer_clear(ringbuffer_handle_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    handle->head = 0;
    handle->tail = 0;

    return RINGBUFFER_OK;
}

/**
 * @brief 重置缓冲区（更换底层缓冲区）
 */
ringbuffer_status_t ringbuffer_reset(ringbuffer_handle_t *handle, uint8_t *buffer, uint16_t size)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    if (buffer == NULL || size < 2)
    {
        return RINGBUFFER_ERROR;
    }

    handle->buffer = buffer;
    handle->size = size;
    handle->head = 0;
    handle->tail = 0;

    return RINGBUFFER_OK;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 检查句柄是否有效
 */
static bool ringbuffer_is_valid(ringbuffer_handle_t *handle)
{
    if (handle == NULL)
    {
        return false;
    }

    if (!handle->is_registered)
    {
        return false;
    }

    if (handle->buffer == NULL || handle->size < 2)
    {
        return false;
    }

    return true;
}

