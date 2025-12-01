/**
  ******************************************************************************
  * @file    ringbuffer.h
  * @brief   环形缓冲区服务层头文件（面向对象风格）
  *          支持动态注册，注册时指定缓冲区大小
  * @version V2.0.0
  * @date    2025-12-01
  * 
  * @example 使用示例：
  *          static uint8_t uart2_buf[256];
  *          ringbuffer_t *uart2_rb = ringbuffer_register(uart2_buf, sizeof(uart2_buf));
  *          
  *          // 面向对象调用方式
  *          uart2_rb->write_byte(uart2_rb, 0x55);
  *          uart2_rb->read(uart2_rb, buf, len);
  *          uint16_t count = uart2_rb->get_count(uart2_rb);
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
 * @brief 环形缓冲区句柄结构定义（面向对象风格）
 */
 typedef struct  {
    /* ========== 私有数据成员 ========== */
    uint8_t *buffer;            /**< 缓冲区指针（用户提供） */
    uint16_t size;              /**< 缓冲区总大小 */
    volatile uint16_t head;     /**< 写入位置（生产者） */
    volatile uint16_t tail;     /**< 读取位置（消费者） */
    bool is_registered;         /**< 注册状态标志 */

    /* ========== 公共方法（函数指针） ========== */
    
    /**
     * @brief 写入单个字节
     * @param self 自身指针
     * @param data 要写入的字节
     * @return RINGBUFFER_OK: 成功, RINGBUFFER_FULL: 缓冲区满
     */
    ringbuffer_status_t (*write_byte)(ringbuffer_t *self, uint8_t data);

    /**
     * @brief 写入多个字节
     * @param self 自身指针
     * @param data 数据指针
     * @param len 数据长度
     * @return 实际写入的字节数
     */
    uint16_t (*write)(ringbuffer_t *self, const uint8_t *data, uint16_t len);

    /**
     * @brief 读取单个字节
     * @param self 自身指针
     * @param data 存储读取数据的指针
     * @return RINGBUFFER_OK: 成功, RINGBUFFER_EMPTY: 缓冲区空
     */
    ringbuffer_status_t (*read_byte)(ringbuffer_t *self, uint8_t *data);

    /**
     * @brief 读取多个字节
     * @param self 自身指针
     * @param data 数据缓冲区指针
     * @param len 要读取的长度
     * @return 实际读取的字节数
     */
    uint16_t (*read)(ringbuffer_t *self, uint8_t *data, uint16_t len);

    /**
     * @brief 查看单个字节（不移除）
     * @param self 自身指针
     * @param data 存储数据的指针
     * @return RINGBUFFER_OK: 成功, RINGBUFFER_EMPTY: 缓冲区空
     */
    ringbuffer_status_t (*peek)(ringbuffer_t *self, uint8_t *data);

    /**
     * @brief 查看多个字节（不移除）
     * @param self 自身指针
     * @param data 数据缓冲区指针
     * @param len 要查看的长度
     * @return 实际可查看的字节数
     */
    uint16_t (*peek_multiple)(ringbuffer_t *self, uint8_t *data, uint16_t len);

    /**
     * @brief 丢弃指定数量的字节
     * @param self 自身指针
     * @param len 要丢弃的字节数
     * @return 实际丢弃的字节数
     */
    uint16_t (*discard)(ringbuffer_t *self, uint16_t len);

    /**
     * @brief 获取缓冲区中可读数据量
     * @param self 自身指针
     * @return 可读字节数
     */
    uint16_t (*get_count)(ringbuffer_t *self);

    /**
     * @brief 获取缓冲区剩余空间
     * @param self 自身指针
     * @return 可写字节数
     */
    uint16_t (*get_free)(ringbuffer_t *self);

    /**
     * @brief 获取缓冲区总大小
     * @param self 自身指针
     * @return 缓冲区总大小
     */
    uint16_t (*get_size)(ringbuffer_t *self);

    /**
     * @brief 检查缓冲区是否为空
     * @param self 自身指针
     * @return true: 空, false: 非空
     */
    bool (*is_empty)(ringbuffer_t *self);

    /**
     * @brief 检查缓冲区是否已满
     * @param self 自身指针
     * @return true: 满, false: 未满
     */
    bool (*is_full)(ringbuffer_t *self);

    /**
     * @brief 清空缓冲区
     * @param self 自身指针
     * @return RINGBUFFER_OK: 成功
     */
    ringbuffer_status_t (*clear)(ringbuffer_t *self);

    /**
     * @brief 重置缓冲区（更换底层缓冲区）
     * @param self 自身指针
     * @param buffer 新的缓冲区指针
     * @param size 新的缓冲区大小
     * @return RINGBUFFER_OK: 成功
     */
    ringbuffer_status_t (*reset)(ringbuffer_t *self, uint8_t *buffer, uint16_t size);
} ringbuffer_t ;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief 初始化环形缓冲区服务
 * @note  可选调用，首次注册会自动初始化
 */
void ringbuffer_init(void);

/**
 * @brief 注册一个环形缓冲区实例
 * @param buffer 用户提供的缓冲区指针
 * @param size 缓冲区大小（字节）
 * @return 成功返回实例指针，失败返回NULL
 * @note  缓冲区内存由用户管理（静态分配或动态分配）
 * 
 * @example
 *     static uint8_t uart2_buf[256];
 *     ringbuffer_t *uart2 = ringbuffer_register(uart2_buf, sizeof(uart2_buf));
 *     
 *     // 面向对象调用
 *     uart2->write_byte(uart2, 0xAA);
 *     uart2->write(uart2, data, len);
 */
ringbuffer_t *ringbuffer_register(uint8_t *buffer, uint16_t size);

/**
 * @brief 注销环形缓冲区实例
 * @param handle 实例指针
 * @return RINGBUFFER_OK: 成功, 其他: 失败
 */
ringbuffer_status_t ringbuffer_unregister(ringbuffer_t *handle);

/* Convenience macros --------------------------------------------------------*/

/**
 * @brief 便捷宏（可选，省略 self 参数）
 * @note  如果觉得每次传 self 麻烦，可以使用这些宏
 */
#define RB_WRITE_BYTE(rb, data)         (rb)->write_byte((rb), (data))
#define RB_WRITE(rb, data, len)         (rb)->write((rb), (data), (len))
#define RB_READ_BYTE(rb, data)          (rb)->read_byte((rb), (data))
#define RB_READ(rb, data, len)          (rb)->read((rb), (data), (len))
#define RB_PEEK(rb, data)               (rb)->peek((rb), (data))
#define RB_GET_COUNT(rb)                (rb)->get_count((rb))
#define RB_GET_FREE(rb)                 (rb)->get_free((rb))
#define RB_IS_EMPTY(rb)                 (rb)->is_empty((rb))
#define RB_IS_FULL(rb)                  (rb)->is_full((rb))
#define RB_CLEAR(rb)                    (rb)->clear((rb))

#ifdef __cplusplus
}
#endif

#endif /* __RINGBUFFER_H__ */
