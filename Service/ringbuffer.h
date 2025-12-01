/**
  ******************************************************************************
  * @file    ringbuffer.h
  * @brief   环形缓冲区服务层头文件
  *          支持动态注册，注册时指定缓冲区大小
  * @version V1.0.0
  * @date    2025-12-01
  ******************************************************************************
  */

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Configuration -------------------------------------------------------------*/

/* 最大支持的环形缓冲区实例数量 */
#ifndef RINGBUFFER_MAX_INSTANCES
#define RINGBUFFER_MAX_INSTANCES    4
#endif

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 环形缓冲区状态枚举
 */
typedef enum {
    RINGBUFFER_OK       = 0,    /**< 操作成功 */
    RINGBUFFER_ERROR    = -1,   /**< 一般错误 */
    RINGBUFFER_FULL     = -2,   /**< 缓冲区已满 */
    RINGBUFFER_EMPTY    = -3,   /**< 缓冲区为空 */
    RINGBUFFER_NOMEM    = -4,   /**< 无可用实例 */
    RINGBUFFER_INVALID  = -5    /**< 无效句柄 */
} ringbuffer_status_t;

/**
 * @brief 环形缓冲区句柄结构（前向声明）
 */
typedef struct ringbuffer_handle ringbuffer_handle_t;

/**
 * @brief 环形缓冲区句柄结构定义
 */
struct ringbuffer_handle {
    uint8_t *buffer;            /**< 缓冲区指针（用户提供） */
    uint16_t size;              /**< 缓冲区总大小 */
    volatile uint16_t head;     /**< 写入位置（生产者） */
    volatile uint16_t tail;     /**< 读取位置（消费者） */
    bool is_registered;         /**< 注册状态标志 */
};

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief 初始化环形缓冲区服务
 * @note  必须在使用其他API前调用
 */
void ringbuffer_init(void);

/**
 * @brief 注册一个环形缓冲区实例
 * @param buffer 用户提供的缓冲区指针
 * @param size 缓冲区大小（字节）
 * @return 成功返回句柄指针，失败返回NULL
 * @note  缓冲区内存由用户管理（静态分配或动态分配）
 */
ringbuffer_handle_t *ringbuffer_register(uint8_t *buffer, uint16_t size);

/**
 * @brief 注销环形缓冲区实例
 * @param handle 句柄指针
 * @return RINGBUFFER_OK: 成功, 其他: 失败
 */
ringbuffer_status_t ringbuffer_unregister(ringbuffer_handle_t *handle);

/**
 * @brief 写入单个字节
 * @param handle 句柄指针
 * @param data 要写入的字节
 * @return RINGBUFFER_OK: 成功, RINGBUFFER_FULL: 缓冲区满
 */
ringbuffer_status_t ringbuffer_write_byte(ringbuffer_handle_t *handle, uint8_t data);

/**
 * @brief 写入多个字节
 * @param handle 句柄指针
 * @param data 数据指针
 * @param len 数据长度
 * @return 实际写入的字节数
 */
uint16_t ringbuffer_write(ringbuffer_handle_t *handle, const uint8_t *data, uint16_t len);

/**
 * @brief 读取单个字节
 * @param handle 句柄指针
 * @param data 存储读取数据的指针
 * @return RINGBUFFER_OK: 成功, RINGBUFFER_EMPTY: 缓冲区空
 */
ringbuffer_status_t ringbuffer_read_byte(ringbuffer_handle_t *handle, uint8_t *data);

/**
 * @brief 读取多个字节
 * @param handle 句柄指针
 * @param data 数据缓冲区指针
 * @param len 要读取的长度
 * @return 实际读取的字节数
 */
uint16_t ringbuffer_read(ringbuffer_handle_t *handle, uint8_t *data, uint16_t len);

/**
 * @brief 查看单个字节（不移除）
 * @param handle 句柄指针
 * @param data 存储数据的指针
 * @return RINGBUFFER_OK: 成功, RINGBUFFER_EMPTY: 缓冲区空
 */
ringbuffer_status_t ringbuffer_peek(ringbuffer_handle_t *handle, uint8_t *data);

/**
 * @brief 查看多个字节（不移除）
 * @param handle 句柄指针
 * @param data 数据缓冲区指针
 * @param len 要查看的长度
 * @return 实际可查看的字节数
 */
uint16_t ringbuffer_peek_multiple(ringbuffer_handle_t *handle, uint8_t *data, uint16_t len);

/**
 * @brief 丢弃指定数量的字节
 * @param handle 句柄指针
 * @param len 要丢弃的字节数
 * @return 实际丢弃的字节数
 */
uint16_t ringbuffer_discard(ringbuffer_handle_t *handle, uint16_t len);

/**
 * @brief 获取缓冲区中可读数据量
 * @param handle 句柄指针
 * @return 可读字节数
 */
uint16_t ringbuffer_get_count(ringbuffer_handle_t *handle);

/**
 * @brief 获取缓冲区剩余空间
 * @param handle 句柄指针
 * @return 可写字节数
 */
uint16_t ringbuffer_get_free(ringbuffer_handle_t *handle);

/**
 * @brief 获取缓冲区总大小
 * @param handle 句柄指针
 * @return 缓冲区总大小
 */
uint16_t ringbuffer_get_size(ringbuffer_handle_t *handle);

/**
 * @brief 检查缓冲区是否为空
 * @param handle 句柄指针
 * @return true: 空, false: 非空
 */
bool ringbuffer_is_empty(ringbuffer_handle_t *handle);

/**
 * @brief 检查缓冲区是否已满
 * @param handle 句柄指针
 * @return true: 满, false: 未满
 */
bool ringbuffer_is_full(ringbuffer_handle_t *handle);

/**
 * @brief 清空缓冲区
 * @param handle 句柄指针
 * @return RINGBUFFER_OK: 成功, RINGBUFFER_INVALID: 无效句柄
 */
ringbuffer_status_t ringbuffer_clear(ringbuffer_handle_t *handle);

/**
 * @brief 重置缓冲区（更换底层缓冲区）
 * @param handle 句柄指针
 * @param buffer 新的缓冲区指针
 * @param size 新的缓冲区大小
 * @return RINGBUFFER_OK: 成功, RINGBUFFER_INVALID: 无效句柄
 */
ringbuffer_status_t ringbuffer_reset(ringbuffer_handle_t *handle, uint8_t *buffer, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RINGBUFFER_H__ */

