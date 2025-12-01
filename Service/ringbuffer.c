/**
  ******************************************************************************
  * @file    ringbuffer.c
  * @brief   环形缓冲区服务层实现文件（面向对象风格）
  *          支持动态注册，注册时指定缓冲区大小
  * @version V2.0.0
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
static ringbuffer_t s_ringbuffer_pool[RINGBUFFER_MAX_INSTANCES];

/**
 * @brief 初始化标志
 */
static bool s_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static bool ringbuffer_is_valid(ringbuffer_t *self);

/* 方法实现（将绑定到函数指针） */
static ringbuffer_status_t _write_byte(ringbuffer_t *self, uint8_t data);
static uint16_t _write(ringbuffer_t *self, const uint8_t *data, uint16_t len);
static ringbuffer_status_t _read_byte(ringbuffer_t *self, uint8_t *data);
static uint16_t _read(ringbuffer_t *self, uint8_t *data, uint16_t len);
static ringbuffer_status_t _peek(ringbuffer_t *self, uint8_t *data);
static uint16_t _peek_multiple(ringbuffer_t *self, uint8_t *data, uint16_t len);
static uint16_t _discard(ringbuffer_t *self, uint16_t len);
static uint16_t _get_count(ringbuffer_t *self);
static uint16_t _get_free(ringbuffer_t *self);
static uint16_t _get_size(ringbuffer_t *self);
static bool _is_empty(ringbuffer_t *self);
static bool _is_full(ringbuffer_t *self);
static ringbuffer_status_t _clear(ringbuffer_t *self);
static ringbuffer_status_t _reset(ringbuffer_t *self, uint8_t *buffer, uint16_t size);

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
ringbuffer_t *ringbuffer_register(uint8_t *buffer, uint16_t size)
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
            ringbuffer_t *rb = &s_ringbuffer_pool[i];

            // 初始化数据成员
            rb->buffer = buffer;
            rb->size = size;
            rb->head = 0;
            rb->tail = 0;
            rb->is_registered = true;

            // 绑定方法（函数指针）
            rb->write_byte = _write_byte;
            rb->write = _write;
            rb->read_byte = _read_byte;
            rb->read = _read;
            rb->peek = _peek;
            rb->peek_multiple = _peek_multiple;
            rb->discard = _discard;
            rb->get_count = _get_count;
            rb->get_free = _get_free;
            rb->get_size = _get_size;
            rb->is_empty = _is_empty;
            rb->is_full = _is_full;
            rb->clear = _clear;
            rb->reset = _reset;

            return rb;
        }
    }

    // 无可用实例
    return NULL;
}

/**
 * @brief 注销环形缓冲区实例
 */
ringbuffer_status_t ringbuffer_unregister(ringbuffer_t *handle)
{
    if (!ringbuffer_is_valid(handle))
    {
        return RINGBUFFER_INVALID;
    }

    // 清除实例
    memset(handle, 0, sizeof(ringbuffer_t));

    return RINGBUFFER_OK;
}

/* Private method implementations --------------------------------------------*/

/**
 * @brief 写入单个字节
 */
static ringbuffer_status_t _write_byte(ringbuffer_t *self, uint8_t data)
{
    if (!ringbuffer_is_valid(self))
    {
        return RINGBUFFER_INVALID;
    }

    uint16_t next_head = (self->head + 1) % self->size;

    // 检查是否已满
    if (next_head == self->tail)
    {
        return RINGBUFFER_FULL;
    }

    // 写入数据
    self->buffer[self->head] = data;
    self->head = next_head;

    return RINGBUFFER_OK;
}

/**
 * @brief 写入多个字节
 */
static uint16_t _write(ringbuffer_t *self, const uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(self) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t written = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (_write_byte(self, data[i]) != RINGBUFFER_OK)
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
static ringbuffer_status_t _read_byte(ringbuffer_t *self, uint8_t *data)
{
    if (!ringbuffer_is_valid(self))
    {
        return RINGBUFFER_INVALID;
    }

    if (data == NULL)
    {
        return RINGBUFFER_ERROR;
    }

    // 检查是否为空
    if (self->head == self->tail)
    {
        return RINGBUFFER_EMPTY;
    }

    // 读取数据
    *data = self->buffer[self->tail];
    self->tail = (self->tail + 1) % self->size;

    return RINGBUFFER_OK;
}

/**
 * @brief 读取多个字节
 */
static uint16_t _read(ringbuffer_t *self, uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(self) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t read_count = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (_read_byte(self, &data[i]) != RINGBUFFER_OK)
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
static ringbuffer_status_t _peek(ringbuffer_t *self, uint8_t *data)
{
    if (!ringbuffer_is_valid(self))
    {
        return RINGBUFFER_INVALID;
    }

    if (data == NULL)
    {
        return RINGBUFFER_ERROR;
    }

    // 检查是否为空
    if (self->head == self->tail)
    {
        return RINGBUFFER_EMPTY;
    }

    // 读取数据但不移动tail
    *data = self->buffer[self->tail];

    return RINGBUFFER_OK;
}

/**
 * @brief 查看多个字节（不移除）
 */
static uint16_t _peek_multiple(ringbuffer_t *self, uint8_t *data, uint16_t len)
{
    if (!ringbuffer_is_valid(self) || data == NULL || len == 0)
    {
        return 0;
    }

    uint16_t count = _get_count(self);
    uint16_t peek_len = (len < count) ? len : count;
    uint16_t temp_tail = self->tail;

    for (uint16_t i = 0; i < peek_len; i++)
    {
        data[i] = self->buffer[temp_tail];
        temp_tail = (temp_tail + 1) % self->size;
    }

    return peek_len;
}

/**
 * @brief 丢弃指定数量的字节
 */
static uint16_t _discard(ringbuffer_t *self, uint16_t len)
{
    if (!ringbuffer_is_valid(self) || len == 0)
    {
        return 0;
    }

    uint16_t count = _get_count(self);
    uint16_t discard_len = (len < count) ? len : count;

    self->tail = (self->tail + discard_len) % self->size;

    return discard_len;
}

/**
 * @brief 获取缓冲区中可读数据量
 */
static uint16_t _get_count(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return 0;
    }

    if (self->head >= self->tail)
    {
        return self->head - self->tail;
    }
    else
    {
        return self->size - self->tail + self->head;
    }
}

/**
 * @brief 获取缓冲区剩余空间
 */
static uint16_t _get_free(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return 0;
    }

    // 环形缓冲区实际可用空间为 size - 1（需要保留一个位置区分满和空）
    return (self->size - 1) - _get_count(self);
}

/**
 * @brief 获取缓冲区总大小
 */
static uint16_t _get_size(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return 0;
    }

    return self->size;
}

/**
 * @brief 检查缓冲区是否为空
 */
static bool _is_empty(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return true;
    }

    return (self->head == self->tail);
}

/**
 * @brief 检查缓冲区是否已满
 */
static bool _is_full(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return true;
    }

    uint16_t next_head = (self->head + 1) % self->size;
    return (next_head == self->tail);
}

/**
 * @brief 清空缓冲区
 */
static ringbuffer_status_t _clear(ringbuffer_t *self)
{
    if (!ringbuffer_is_valid(self))
    {
        return RINGBUFFER_INVALID;
    }

    self->head = 0;
    self->tail = 0;

    return RINGBUFFER_OK;
}

/**
 * @brief 重置缓冲区（更换底层缓冲区）
 */
static ringbuffer_status_t _reset(ringbuffer_t *self, uint8_t *buffer, uint16_t size)
{
    if (!ringbuffer_is_valid(self))
    {
        return RINGBUFFER_INVALID;
    }

    if (buffer == NULL || size < 2)
    {
        return RINGBUFFER_ERROR;
    }

    self->buffer = buffer;
    self->size = size;
    self->head = 0;
    self->tail = 0;

    return RINGBUFFER_OK;
}

/* Private helper functions --------------------------------------------------*/

/**
 * @brief 检查句柄是否有效
 */
static bool ringbuffer_is_valid(ringbuffer_t *self)
{
    if (self == NULL)
    {
        return false;
    }

    if (!self->is_registered)
    {
        return false;
    }

    if (self->buffer == NULL || self->size < 2)
    {
        return false;
    }

    return true;
}
