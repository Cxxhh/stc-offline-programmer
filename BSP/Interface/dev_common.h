/**
  ******************************************************************************
  * @file    dev_common.h
  * @brief   通用设备接口基类定义
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

#ifndef __DEV_COMMON_H__
#define __DEV_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/* 设备状态枚举 */
typedef enum {
    DEV_OK       = 0,
    DEV_ERROR    = -1,
    DEV_BUSY     = -2,
    DEV_TIMEOUT  = -3,
    DEV_NODEV    = -4,
    DEV_INVALID  = -5
} dev_status_t;

/* 设备类型枚举 */
typedef enum {
    DEV_TYPE_SERIAL = 0,    /* 串口设备 */
    DEV_TYPE_DISPLAY,       /* 显示设备 */
    DEV_TYPE_STORAGE,       /* 存储设备 */
    DEV_TYPE_GPIO,          /* GPIO设备 */
    DEV_TYPE_MAX
} dev_type_t;

/* 设备基类结构（所有设备继承此结构） */
typedef struct dev_base {
    const char *name;                           /* 设备名称 */
    dev_type_t type;                            /* 设备类型 */
    bool is_open;                               /* 打开状态 */
    void *priv_data;                            /* 私有数据指针 */
    
    /* 基础操作接口（必须实现） */
    dev_status_t (*open)(struct dev_base *dev);
    dev_status_t (*close)(struct dev_base *dev);
    dev_status_t (*ioctl)(struct dev_base *dev, uint32_t cmd, void *arg);
} dev_base_t;

/* Exported defines ----------------------------------------------------------*/

/* IOCTL通用命令定义 */
#define DEV_IOCTL_GET_STATUS    0x0001
#define DEV_IOCTL_RESET         0x0002
#define DEV_IOCTL_FLUSH         0x0003

#ifdef __cplusplus
}
#endif

#endif /* __DEV_COMMON_H__ */

