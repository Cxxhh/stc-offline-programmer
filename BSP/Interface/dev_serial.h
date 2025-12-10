/**
  ******************************************************************************
  * @file    dev_serial.h
  * @brief   串口设备接口定义
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

#ifndef __DEV_SERIAL_H__
#define __DEV_SERIAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "dev_common.h"

/* Exported types ------------------------------------------------------------*/

/* 串口校验位枚举 */
typedef enum {
    SERIAL_PARITY_NONE = 0,
    SERIAL_PARITY_EVEN,
    SERIAL_PARITY_ODD
} serial_parity_t;

/* 串口配置结构 */
typedef struct {
    uint32_t baudrate;          /* 波特率 */
    uint8_t  databits;          /* 数据位 (8/9) */
    uint8_t  stopbits;          /* 停止位 (1/2) */
    serial_parity_t parity;     /* 校验位 */
} serial_config_t;

/* 串口设备结构（继承dev_base_t） */
typedef struct serial_dev {
    dev_base_t base;            /* 基类（必须放在首位） */
    
    /* 串口专用操作接口 */
    dev_status_t (*write)(struct serial_dev *dev, const uint8_t *data, uint16_t len);
    dev_status_t (*read)(struct serial_dev *dev, uint8_t *data, uint16_t len, uint16_t *actual);
    dev_status_t (*set_config)(struct serial_dev *dev, const serial_config_t *config);
    dev_status_t (*get_config)(struct serial_dev *dev, serial_config_t *config);
    
    /* 异步操作接口 */
    uint16_t (*get_rx_count)(struct serial_dev *dev);
    void (*flush_rx)(struct serial_dev *dev);
    void (*flush_tx)(struct serial_dev *dev);
    
    /* 回调函数 */
    void (*rx_callback)(struct serial_dev *dev, uint8_t data);
    void (*tx_complete_callback)(struct serial_dev *dev);
    
    /* 配置数据 */
    serial_config_t config;
    uint32_t timeout_ms;
} serial_dev_t;

/* Exported defines ----------------------------------------------------------*/

/* 串口设备IOCTL命令 */
#define SERIAL_IOCTL_SET_BAUDRATE   0x1001
#define SERIAL_IOCTL_SET_PARITY     0x1002
#define SERIAL_IOCTL_GET_RX_COUNT   0x1003
#define SERIAL_IOCTL_FLUSH_RX       0x1004
#define SERIAL_IOCTL_FLUSH_TX       0x1005
#define SERIAL_IOCTL_SET_TIMEOUT    0x1006

/* 便捷宏定义 */
#define SERIAL_WRITE(dev, data, len)        (dev)->write((dev), (data), (len))
#define SERIAL_READ(dev, data, len, actual) (dev)->read((dev), (data), (len), (actual))
#define SERIAL_SET_BAUDRATE(dev, baud)      (dev)->base.ioctl(&(dev)->base, SERIAL_IOCTL_SET_BAUDRATE, (void*)(uintptr_t)(baud))

#ifdef __cplusplus
}
#endif

#endif /* __DEV_SERIAL_H__ */

