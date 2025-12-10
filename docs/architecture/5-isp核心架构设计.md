# 5. ISP核心架构设计

### 5.1 ISP状态机

```
┌─────────────────────────────────────────────────────────────────┐
│                      ISP State Machine                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│    ┌──────┐    握手成功    ┌──────────┐    信息解析    ┌──────┐ │
│    │ IDLE │───────────────▶│ CONNECTED│───────────────▶│ READY│ │
│    └──┬───┘                └────┬─────┘                └──┬───┘ │
│       │                         │                         │      │
│       │ 开始握手                │ 超时/错误               │      │
│       ▼                         ▼                         │      │
│    ┌──────────┐           ┌─────────┐                    │      │
│    │HANDSHAKE │◀──────────│  ERROR  │◀───────────────────┤      │
│    └──────────┘  重试      └─────────┘   任何步骤失败      │      │
│                                                           │      │
│                                          开始擦除         │      │
│    ┌──────────┐    擦除完成    ┌─────────┐◀──────────────┘      │
│    │ ERASING  │───────────────▶│ ERASED  │                      │
│    └────┬─────┘                └────┬────┘                      │
│         │                           │                            │
│         │ 超时                      │ 开始编程                    │
│         ▼                           ▼                            │
│    ┌─────────┐                ┌───────────┐    编程完成          │
│    │  ERROR  │                │PROGRAMMING│───────────────┐      │
│    └─────────┘                └─────┬─────┘               │      │
│                                     │                     ▼      │
│                                     │ 超时          ┌──────────┐ │
│                                     ▼               │VERIFYING │ │
│                               ┌─────────┐           └────┬─────┘ │
│                               │  ERROR  │                │       │
│                               └─────────┘                │校验通过│
│                                                          ▼       │
│                                                    ┌──────────┐  │
│                                                    │ COMPLETE │  │
│                                                    └──────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 ISP核心接口定义

```c
/* Service/isp_core/stc_isp_core.h */

#ifndef __STC_ISP_CORE_H__
#define __STC_ISP_CORE_H__

#include "BSP/Interface/dev_serial.h"
#include "Middlewares/stc_isp/stc_mcu_database.h"

/* ISP状态枚举 */
typedef enum {
    ISP_STATE_IDLE = 0,
    ISP_STATE_HANDSHAKE,
    ISP_STATE_CONNECTED,
    ISP_STATE_READY,
    ISP_STATE_ERASING,
    ISP_STATE_ERASED,
    ISP_STATE_PROGRAMMING,
    ISP_STATE_VERIFYING,
    ISP_STATE_COMPLETE,
    ISP_STATE_ERROR
} isp_state_t;

/* ISP错误码 */
typedef enum {
    ISP_OK = 0,
    ISP_ERR_TIMEOUT,
    ISP_ERR_CHECKSUM,
    ISP_ERR_PROTOCOL,
    ISP_ERR_UNSUPPORTED,
    ISP_ERR_VERIFY,
    ISP_ERR_NO_DEVICE
} isp_error_t;

/* ISP事件类型（回调通知） */
typedef enum {
    ISP_EVENT_HANDSHAKE_START,
    ISP_EVENT_HANDSHAKE_OK,
    ISP_EVENT_MCU_DETECTED,
    ISP_EVENT_ERASE_START,
    ISP_EVENT_ERASE_DONE,
    ISP_EVENT_PROGRAM_START,
    ISP_EVENT_PROGRAM_PROGRESS,
    ISP_EVENT_PROGRAM_DONE,
    ISP_EVENT_VERIFY_START,
    ISP_EVENT_VERIFY_DONE,
    ISP_EVENT_COMPLETE,
    ISP_EVENT_ERROR
} isp_event_t;

/* 事件回调参数 */
typedef struct {
    isp_event_t event;
    union {
        struct {
            const stc_mcu_model_t *model;
            uint8_t uid[7];
        } mcu_info;
        struct {
            uint32_t current;
            uint32_t total;
        } progress;
        struct {
            isp_error_t code;
            const char *message;
        } error;
    } data;
} isp_event_data_t;

/* 事件回调函数类型 */
typedef void (*isp_event_callback_t)(const isp_event_data_t *event);

/* ISP配置结构 */
typedef struct {
    uint32_t handshake_timeout_ms;      /* 握手超时 */
    uint32_t erase_timeout_ms;          /* 擦除超时 */
    uint32_t program_timeout_ms;        /* 编程超时 */
    uint8_t  retry_count;               /* 重试次数 */
    uint32_t target_baudrate;           /* 目标波特率 */
} isp_config_t;

/* ISP句柄结构 */
typedef struct {
    serial_dev_t *serial;               /* 串口设备（依赖注入） */
    ringbuffer_t *rx_buffer;            /* 接收缓冲区 */
    isp_state_t   state;                /* 当前状态 */
    isp_error_t   last_error;           /* 最后错误 */
    isp_config_t  config;               /* 配置参数 */
    isp_event_callback_t callback;      /* 事件回调 */
    
    /* 目标MCU信息 */
    const stc_mcu_model_t *target_mcu;
    uint8_t target_uid[7];
    uint32_t negotiated_baudrate;
} isp_handle_t;

/* ==================== API接口 ==================== */

/* 初始化ISP句柄 */
isp_error_t isp_init(isp_handle_t *handle, serial_dev_t *serial, ringbuffer_t *rx_buf);

/* 设置配置参数 */
void isp_set_config(isp_handle_t *handle, const isp_config_t *config);

/* 注册事件回调 */
void isp_set_callback(isp_handle_t *handle, isp_event_callback_t callback);

/* 启动握手 */
isp_error_t isp_start_handshake(isp_handle_t *handle);

/* 执行擦除 */
isp_error_t isp_erase(isp_handle_t *handle);

/* 执行编程 */
isp_error_t isp_program(isp_handle_t *handle, const uint8_t *data, uint32_t size, uint32_t addr);

/* 执行校验 */
isp_error_t isp_verify(isp_handle_t *handle, const uint8_t *data, uint32_t size, uint32_t addr);

/* 一键烧录（擦除+编程+校验） */
isp_error_t isp_flash(isp_handle_t *handle, const uint8_t *data, uint32_t size);

/* 获取当前状态 */
isp_state_t isp_get_state(isp_handle_t *handle);

/* 获取目标MCU信息 */
const stc_mcu_model_t *isp_get_target_mcu(isp_handle_t *handle);

/* 取消当前操作 */
void isp_abort(isp_handle_t *handle);

/* 复位状态机 */
void isp_reset(isp_handle_t *handle);

/* 主循环处理（非阻塞模式下调用） */
void isp_process(isp_handle_t *handle);

#endif /* __STC_ISP_CORE_H__ */
```

---
