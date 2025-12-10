# STC单片机离线烧录器 系统架构设计文档

**版本**: 1.0.0  
**创建日期**: 2025-12-10  
**架构师**: Winston  
**目标**: 高执行效率、快速重定向、多平台兼容、高度解耦、可移植性

---

## 1. 架构概览

### 1.1 设计原则

| 原则 | 描述 | 实现方式 |
|------|------|----------|
| **接口抽象** | 所有硬件操作通过统一接口访问 | 设备操作结构体 + 函数指针 |
| **依赖注入** | 上层模块不直接依赖具体实现 | 运行时注册设备实例 |
| **单向依赖** | 只允许上层调用下层 | 严格分层 + 编译检查 |
| **零耦合移植** | 更换平台只需重写Port层 | Port/Driver分离 |
| **统一IO模型** | 串口/LCD/存储使用相同操作范式 | 通用设备接口 |

### 1.2 分层架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              APP Layer                                   │
│                    (app_main, app_ui, app_isp)                          │
│                      业务逻辑 / UI交互 / 流程控制                          │
├─────────────────────────────────────────────────────────────────────────┤
│                           Service Layer                                  │
│              ┌─────────────┬─────────────┬─────────────┐                │
│              │  ISP Core   │  File Mgr   │   Config    │                │
│              │  烧录状态机  │  固件管理   │   配置管理   │                │
│              └─────────────┴─────────────┴─────────────┘                │
├─────────────────────────────────────────────────────────────────────────┤
│                         Middlewares Layer                                │
│       ┌──────────┬──────────┬──────────┬──────────┬──────────┐         │
│       │  FatFs   │   Log    │ RingBuf  │ Protocol │  MCU DB  │         │
│       │ 文件系统  │  日志    │ 环形缓冲  │ ISP协议  │ 型号数据  │         │
│       └──────────┴──────────┴──────────┴──────────┴──────────┘         │
├─────────────────────────────────────────────────────────────────────────┤
│                         Device Layer (BSP)                               │
│       ┌─────────────────────────────────────────────────────────┐       │
│       │              Unified Device Interface                    │       │
│       │                 统一设备接口抽象                          │       │
│       ├─────────────────────────────────────────────────────────┤       │
│       │   Serial Dev   │   Display Dev   │   Storage Dev        │       │
│       │   串口设备      │    显示设备      │    存储设备          │       │
│       │   (UART/USB)   │   (LCD/OLED)    │   (SD/Flash)         │       │
│       └─────────────────────────────────────────────────────────┘       │
├─────────────────────────────────────────────────────────────────────────┤
│                      Platform Port Layer (HAL)                           │
│       ┌─────────────────────────────────────────────────────────┐       │
│       │                  Port Interface                          │       │
│       │               平台移植接口层                              │       │
│       ├───────────────┬───────────────┬─────────────────────────┤       │
│       │  STM32G4 Port │  STM32F1 Port │   Other MCU Port        │       │
│       │   (LL库实现)   │   (LL库实现)  │     (待扩展)            │       │
│       └───────────────┴───────────────┴─────────────────────────┘       │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.3 目录结构

```
STC/
├── APP/                          # L4 应用层
│   ├── app_main.c/h             # 主应用入口
│   ├── app_ui.c/h               # UI状态机
│   └── app_isp.c/h              # ISP烧录应用
│
├── Service/                      # L3 服务层
│   ├── isp_core/                # ISP核心服务
│   │   ├── stc_isp_core.c/h    # 烧录状态机
│   │   └── stc_isp_protocol.c/h # 协议实现
│   ├── display/                 # 显示服务 ⭐ NEW
│   │   ├── disp_service.c/h    # 高级显示接口
│   │   └── disp_widget.c/h     # UI组件库
│   ├── file_mgr/                # 文件管理服务
│   │   └── file_manager.c/h
│   ├── config/                  # 配置管理
│   │   └── sys_config.c/h
│   ├── log.c/h                  # 日志服务
│   └── ringbuffer.c/h           # 环形缓冲区
│
├── Middlewares/                  # L2 中间件层
│   ├── FatFs/                   # 文件系统
│   └── stc_isp/                 # STC ISP中间件
│       ├── stc_mcu_database.c/h # MCU数据库
│       └── stc_protocol_def.h   # 协议定义
│
├── BSP/                          # L1 设备驱动层
│   ├── Interface/               # 统一设备接口
│   │   ├── dev_serial.h        # 串口设备接口
│   │   ├── dev_display.h       # 显示设备接口
│   │   ├── dev_storage.h       # 存储设备接口
│   │   ├── dev_gpio.h          # GPIO设备接口
│   │   └── dev_common.h        # 公共定义
│   │
│   ├── Serial/                  # 串口设备实现
│   │   ├── bsp_serial.c/h      # 串口设备管理器
│   │   └── bsp_serial_stc.c/h  # STC专用串口封装
│   │
│   ├── Display/                 # 显示设备实现 ⭐ 封装层
│   │   └── bsp_display.c/h     # display_dev_t 接口 → 调用现有 LCD_xxx()
│   │
│   ├── Storage/                 # 存储设备实现
│   │   ├── bsp_storage.c/h     # 存储设备管理器
│   │   └── bsp_sdcard.c/h      # SD卡驱动
│   │
│   └── Key/                     # 按键设备
│       └── bsp_key.c/h
│
├── Port/                         # L0 平台移植层
│   ├── port_def.h               # 移植层公共定义
│   ├── STM32G4/                 # STM32G4平台
│   │   ├── port_uart.c/h       # UART移植
│   │   ├── port_spi.c/h        # SPI移植
│   │   ├── port_gpio.c/h       # GPIO移植
│   │   ├── port_timer.c/h      # 定时器移植
│   │   └── port_system.c/h     # 系统移植(时钟/中断)
│   │
│   └── STM32F1/                 # STM32F1平台(预留)
│       └── ...
│
├── Inc/                          # 全局头文件
│   └── lcd.h                    # ⭐ 现有LCD驱动头文件（不修改）
│
└── Src/                          # CubeMX生成代码 + 现有驱动
    └── lcd.c                    # ⭐ 现有LCD驱动（不修改，直接复用）
```

### 1.4 LCD 驱动集成说明

> ⚠️ **重要前提**：项目已存在稳定可用的 LCD 驱动模块（`Src/lcd.c` + `Inc/lcd.h`），
> 已在现有工程中验证通过。**本项目不重新设计/实现底层 LCD 驱动**。

| 层级 | 模块 | 职责 |
|------|------|------|
| **L4 APP** | `app_ui.c` | 页面逻辑，调用 Display Service |
| **L3 Service** | `disp_service.c` | 高级显示接口（进度条、消息框等） |
| **L1 BSP** | `bsp_display.c` | `display_dev_t` 封装，内部调用 `LCD_xxx()` |
| **L0 Driver** | `Src/lcd.c` ⭐ | **现有驱动，不修改，直接复用** |

调用链路：
```
APP → disp_service_xxx() → display_dev_t → LCD_xxx() → 硬件
```

---

## 2. 核心接口设计

### 2.1 通用设备接口基类

```c
/* BSP/Interface/dev_common.h */

#ifndef __DEV_COMMON_H__
#define __DEV_COMMON_H__

#include <stdint.h>
#include <stdbool.h>

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

/* IOCTL通用命令定义 */
#define DEV_IOCTL_GET_STATUS    0x0001
#define DEV_IOCTL_RESET         0x0002
#define DEV_IOCTL_FLUSH         0x0003

#endif /* __DEV_COMMON_H__ */
```

### 2.2 串口设备接口

```c
/* BSP/Interface/dev_serial.h */

#ifndef __DEV_SERIAL_H__
#define __DEV_SERIAL_H__

#include "dev_common.h"

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

#endif /* __DEV_SERIAL_H__ */
```

### 2.3 显示设备接口

```c
/* BSP/Interface/dev_display.h */

#ifndef __DEV_DISPLAY_H__
#define __DEV_DISPLAY_H__

#include "dev_common.h"

/* RGB565颜色类型 */
typedef uint16_t color_t;

/* 字体结构 */
typedef struct {
    uint8_t width;
    uint8_t height;
    const uint8_t *data;
} font_t;

/* 显示设备结构（继承dev_base_t） */
typedef struct display_dev {
    dev_base_t base;            /* 基类 */
    
    /* 显示属性 */
    uint16_t width;
    uint16_t height;
    uint8_t  rotation;
    
    /* 基础绘图接口 */
    dev_status_t (*clear)(struct display_dev *dev, color_t color);
    dev_status_t (*draw_pixel)(struct display_dev *dev, uint16_t x, uint16_t y, color_t color);
    dev_status_t (*draw_hline)(struct display_dev *dev, uint16_t x, uint16_t y, uint16_t len, color_t color);
    dev_status_t (*draw_vline)(struct display_dev *dev, uint16_t x, uint16_t y, uint16_t len, color_t color);
    dev_status_t (*fill_rect)(struct display_dev *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, color_t color);
    
    /* 高级绘图接口 */
    dev_status_t (*draw_char)(struct display_dev *dev, uint16_t x, uint16_t y, char c, const font_t *font, color_t fg, color_t bg);
    dev_status_t (*draw_string)(struct display_dev *dev, uint16_t x, uint16_t y, const char *str, const font_t *font, color_t fg, color_t bg);
    dev_status_t (*draw_bitmap)(struct display_dev *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
    
    /* 批量写入接口（高性能） */
    dev_status_t (*set_window)(struct display_dev *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    dev_status_t (*write_data)(struct display_dev *dev, const uint16_t *data, uint32_t len);
    
    /* 显示控制 */
    dev_status_t (*set_backlight)(struct display_dev *dev, uint8_t level);
    dev_status_t (*set_rotation)(struct display_dev *dev, uint8_t rotation);
} display_dev_t;

/* 显示设备IOCTL命令 */
#define DISPLAY_IOCTL_SET_BACKLIGHT  0x2001
#define DISPLAY_IOCTL_SET_ROTATION   0x2002
#define DISPLAY_IOCTL_GET_SIZE       0x2003

/* 预定义颜色 */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

#endif /* __DEV_DISPLAY_H__ */
```

### 2.4 存储设备接口

```c
/* BSP/Interface/dev_storage.h */

#ifndef __DEV_STORAGE_H__
#define __DEV_STORAGE_H__

#include "dev_common.h"

/* 存储设备信息 */
typedef struct {
    uint32_t sector_size;       /* 扇区大小 */
    uint32_t sector_count;      /* 扇区数量 */
    uint32_t block_size;        /* 块大小 */
    bool     is_ready;          /* 设备就绪 */
} storage_info_t;

/* 存储设备结构（继承dev_base_t） */
typedef struct storage_dev {
    dev_base_t base;            /* 基类 */
    
    /* 存储操作接口 */
    dev_status_t (*read_sector)(struct storage_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count);
    dev_status_t (*write_sector)(struct storage_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count);
    dev_status_t (*get_info)(struct storage_dev *dev, storage_info_t *info);
    dev_status_t (*sync)(struct storage_dev *dev);
    
    /* 存储信息 */
    storage_info_t info;
} storage_dev_t;

/* 存储设备IOCTL命令 */
#define STORAGE_IOCTL_GET_SECTOR_SIZE   0x3001
#define STORAGE_IOCTL_GET_SECTOR_COUNT  0x3002
#define STORAGE_IOCTL_SYNC              0x3003
#define STORAGE_IOCTL_IS_READY          0x3004

#endif /* __DEV_STORAGE_H__ */
```

---

## 3. 设备管理器设计

### 3.1 设备注册与查找

```c
/* BSP/dev_manager.h */

#ifndef __DEV_MANAGER_H__
#define __DEV_MANAGER_H__

#include "Interface/dev_common.h"
#include "Interface/dev_serial.h"
#include "Interface/dev_display.h"
#include "Interface/dev_storage.h"

/* 最大设备数量 */
#define DEV_MAX_SERIAL      4
#define DEV_MAX_DISPLAY     2
#define DEV_MAX_STORAGE     2

/* 设备管理器初始化 */
void dev_manager_init(void);

/* 设备注册接口 */
dev_status_t dev_register_serial(serial_dev_t *dev);
dev_status_t dev_register_display(display_dev_t *dev);
dev_status_t dev_register_storage(storage_dev_t *dev);

/* 设备注销接口 */
dev_status_t dev_unregister(dev_base_t *dev);

/* 设备查找接口 */
serial_dev_t  *dev_find_serial(const char *name);
display_dev_t *dev_find_display(const char *name);
storage_dev_t *dev_find_storage(const char *name);

/* 默认设备获取（快速访问） */
serial_dev_t  *dev_get_default_serial(void);
display_dev_t *dev_get_default_display(void);
storage_dev_t *dev_get_default_storage(void);

/* 设置默认设备 */
void dev_set_default_serial(serial_dev_t *dev);
void dev_set_default_display(display_dev_t *dev);
void dev_set_default_storage(storage_dev_t *dev);

#endif /* __DEV_MANAGER_H__ */
```

### 3.2 设备快速重定向机制

```c
/* 快速重定向示例代码 */

/* 方式1: 运行时切换默认设备 */
void switch_to_uart2(void)
{
    serial_dev_t *uart2 = dev_find_serial("UART2");
    dev_set_default_serial(uart2);
}

/* 方式2: 通过ISP核心接口注入设备 */
void isp_set_serial_device(serial_dev_t *dev);

/* 方式3: 编译时配置（在port_config.h中定义） */
// #define ISP_DEFAULT_SERIAL    "UART2"
// #define LOG_DEFAULT_SERIAL    "UART1"
```

---

## 4. 平台移植层设计

### 4.1 移植接口定义

```c
/* Port/port_def.h */

#ifndef __PORT_DEF_H__
#define __PORT_DEF_H__

#include <stdint.h>
#include <stdbool.h>

/* ==================== 系统接口 ==================== */

/* 系统初始化 */
void port_system_init(void);

/* 延时函数 */
void port_delay_ms(uint32_t ms);
void port_delay_us(uint32_t us);

/* 系统时钟 */
uint32_t port_get_tick(void);
uint32_t port_get_sysclk(void);

/* 临界区保护 */
uint32_t port_enter_critical(void);
void port_exit_critical(uint32_t state);

/* ==================== UART接口 ==================== */

/* UART端口标识 */
typedef enum {
    PORT_UART1 = 0,
    PORT_UART2,
    PORT_UART3,
    PORT_UART_MAX
} port_uart_id_t;

/* UART配置 */
typedef struct {
    uint32_t baudrate;
    uint8_t  databits;      /* 8 or 9 */
    uint8_t  stopbits;      /* 1 or 2 */
    uint8_t  parity;        /* 0=None, 1=Even, 2=Odd */
} port_uart_config_t;

/* UART回调 */
typedef void (*port_uart_rx_callback_t)(uint8_t data);

/* UART操作接口 */
int  port_uart_init(port_uart_id_t id, const port_uart_config_t *config);
void port_uart_deinit(port_uart_id_t id);
int  port_uart_send(port_uart_id_t id, const uint8_t *data, uint16_t len);
int  port_uart_send_byte(port_uart_id_t id, uint8_t data);
int  port_uart_set_baudrate(port_uart_id_t id, uint32_t baudrate);
int  port_uart_set_parity(port_uart_id_t id, uint8_t parity);
void port_uart_set_rx_callback(port_uart_id_t id, port_uart_rx_callback_t cb);

/* ==================== SPI接口 ==================== */

typedef enum {
    PORT_SPI1 = 0,
    PORT_SPI2,
    PORT_SPI_MAX
} port_spi_id_t;

int  port_spi_init(port_spi_id_t id);
void port_spi_deinit(port_spi_id_t id);
int  port_spi_transfer(port_spi_id_t id, const uint8_t *tx, uint8_t *rx, uint16_t len);
void port_spi_set_speed(port_spi_id_t id, uint32_t speed_hz);
void port_spi_cs_low(port_spi_id_t id, uint8_t cs_pin);
void port_spi_cs_high(port_spi_id_t id, uint8_t cs_pin);

/* ==================== GPIO接口 ==================== */

typedef enum {
    PORT_GPIO_MODE_INPUT = 0,
    PORT_GPIO_MODE_OUTPUT,
    PORT_GPIO_MODE_AF
} port_gpio_mode_t;

typedef enum {
    PORT_GPIO_PULL_NONE = 0,
    PORT_GPIO_PULL_UP,
    PORT_GPIO_PULL_DOWN
} port_gpio_pull_t;

void port_gpio_init(uint8_t port, uint8_t pin, port_gpio_mode_t mode, port_gpio_pull_t pull);
void port_gpio_write(uint8_t port, uint8_t pin, uint8_t state);
uint8_t port_gpio_read(uint8_t port, uint8_t pin);
void port_gpio_toggle(uint8_t port, uint8_t pin);

#endif /* __PORT_DEF_H__ */
```

### 4.2 STM32G4平台实现示例

```c
/* Port/STM32G4/port_uart.c - 关键片段 */

#include "port_def.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_bus.h"

/* UART硬件映射表 */
static const struct {
    USART_TypeDef *instance;
    IRQn_Type      irqn;
    uint32_t       rcc_periph;
} s_uart_map[PORT_UART_MAX] = {
    [PORT_UART1] = { USART1, USART1_IRQn, LL_APB2_GRP1_PERIPH_USART1 },
    [PORT_UART2] = { USART2, USART2_IRQn, LL_APB1_GRP1_PERIPH_USART2 },
    [PORT_UART3] = { USART3, USART3_IRQn, LL_APB1_GRP1_PERIPH_USART3 },
};

/* RX回调函数指针数组 */
static port_uart_rx_callback_t s_uart_rx_cb[PORT_UART_MAX] = {0};

int port_uart_init(port_uart_id_t id, const port_uart_config_t *config)
{
    if (id >= PORT_UART_MAX || config == NULL) {
        return -1;
    }
    
    USART_TypeDef *uart = s_uart_map[id].instance;
    LL_USART_InitTypeDef init = {0};
    
    /* 配置参数 */
    init.BaudRate = config->baudrate;
    init.StopBits = (config->stopbits == 2) ? LL_USART_STOPBITS_2 : LL_USART_STOPBITS_1;
    init.TransferDirection = LL_USART_DIRECTION_TX_RX;
    init.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    init.OverSampling = LL_USART_OVERSAMPLING_16;
    
    /* 校验位配置 */
    if (config->parity == 0) {
        init.DataWidth = LL_USART_DATAWIDTH_8B;
        init.Parity = LL_USART_PARITY_NONE;
    } else {
        init.DataWidth = LL_USART_DATAWIDTH_9B;
        init.Parity = (config->parity == 1) ? LL_USART_PARITY_EVEN : LL_USART_PARITY_ODD;
    }
    
    /* 初始化UART */
    LL_USART_Disable(uart);
    LL_USART_Init(uart, &init);
    LL_USART_Enable(uart);
    
    /* 等待就绪 */
    while (!(LL_USART_IsActiveFlag_TEACK(uart) && LL_USART_IsActiveFlag_REACK(uart)));
    
    /* 使能RX中断 */
    LL_USART_EnableIT_RXNE(uart);
    NVIC_EnableIRQ(s_uart_map[id].irqn);
    
    return 0;
}

int port_uart_set_baudrate(port_uart_id_t id, uint32_t baudrate)
{
    if (id >= PORT_UART_MAX) return -1;
    
    USART_TypeDef *uart = s_uart_map[id].instance;
    
    LL_USART_Disable(uart);
    LL_USART_SetBaudRate(uart, SystemCoreClock, LL_USART_PRESCALER_DIV1, 
                         LL_USART_OVERSAMPLING_16, baudrate);
    LL_USART_Enable(uart);
    
    while (!(LL_USART_IsActiveFlag_TEACK(uart) && LL_USART_IsActiveFlag_REACK(uart)));
    
    return 0;
}

/* 中断处理（在stm32g4xx_it.c中调用） */
void port_uart_irq_handler(port_uart_id_t id)
{
    USART_TypeDef *uart = s_uart_map[id].instance;
    
    if (LL_USART_IsActiveFlag_RXNE(uart)) {
        uint8_t data = LL_USART_ReceiveData8(uart);
        if (s_uart_rx_cb[id]) {
            s_uart_rx_cb[id](data);
        }
    }
}
```

---

## 5. ISP核心架构设计

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

## 6. 依赖注入与解耦示例

### 6.1 应用层使用示例

```c
/* APP/app_isp.c */

#include "app_isp.h"
#include "Service/isp_core/stc_isp_core.h"
#include "BSP/dev_manager.h"
#include "Service/ringbuffer.h"

/* 私有变量 */
static isp_handle_t s_isp_handle;
static uint8_t s_rx_buf[512];
static ringbuffer_t *s_rx_ring;

/* 事件回调 */
static void _isp_event_callback(const isp_event_data_t *event)
{
    display_dev_t *lcd = dev_get_default_display();
    
    switch (event->event) {
    case ISP_EVENT_MCU_DETECTED:
        lcd->draw_string(lcd, 0, 20, event->data.mcu_info.model->name, 
                        &font_8x16, COLOR_GREEN, COLOR_BLACK);
        break;
        
    case ISP_EVENT_PROGRAM_PROGRESS:
        /* 更新进度条 */
        uint8_t percent = (event->data.progress.current * 100) / event->data.progress.total;
        app_ui_update_progress(percent);
        break;
        
    case ISP_EVENT_COMPLETE:
        app_ui_show_result(true, "烧录成功");
        break;
        
    case ISP_EVENT_ERROR:
        app_ui_show_result(false, event->data.error.message);
        break;
    }
}

/* 初始化 */
void app_isp_init(void)
{
    /* 获取设备（依赖注入） */
    serial_dev_t *uart = dev_find_serial("STC_UART");  /* 可配置 */
    
    /* 注册环形缓冲区 */
    s_rx_ring = ringbuffer_register(s_rx_buf, sizeof(s_rx_buf));
    
    /* 初始化ISP核心 */
    isp_init(&s_isp_handle, uart, s_rx_ring);
    isp_set_callback(&s_isp_handle, _isp_event_callback);
    
    /* 配置参数 */
    isp_config_t config = {
        .handshake_timeout_ms = 3000,
        .erase_timeout_ms = 10000,
        .program_timeout_ms = 30000,
        .retry_count = 3,
        .target_baudrate = 115200
    };
    isp_set_config(&s_isp_handle, &config);
}

/* 切换串口设备（运行时重定向） */
void app_isp_switch_uart(const char *uart_name)
{
    serial_dev_t *uart = dev_find_serial(uart_name);
    if (uart) {
        isp_reset(&s_isp_handle);
        isp_init(&s_isp_handle, uart, s_rx_ring);
    }
}

/* 执行烧录 */
void app_isp_flash_firmware(const uint8_t *data, uint32_t size)
{
    isp_flash(&s_isp_handle, data, size);
}
```

### 6.2 设备注册示例（系统启动时）

```c
/* BSP/bsp_init.c */

#include "BSP/dev_manager.h"
#include "BSP/Serial/bsp_serial_stc.h"
#include "BSP/Display/bsp_lcd_st7735.h"
#include "BSP/Storage/bsp_sdcard.h"

void bsp_init(void)
{
    /* 初始化设备管理器 */
    dev_manager_init();
    
    /* 创建并注册串口设备 */
    serial_dev_t *uart1 = bsp_serial_create(PORT_UART1, "LOG_UART");
    serial_dev_t *uart2 = bsp_serial_create(PORT_UART2, "STC_UART");
    dev_register_serial(uart1);
    dev_register_serial(uart2);
    
    /* 创建并注册显示设备 */
    display_dev_t *lcd = bsp_lcd_st7735_create("LCD");
    dev_register_display(lcd);
    
    /* 创建并注册存储设备 */
    storage_dev_t *sd = bsp_sdcard_create("SD");
    dev_register_storage(sd);
    
    /* 设置默认设备 */
    dev_set_default_serial(uart2);      /* STC通信默认用UART2 */
    dev_set_default_display(lcd);
    dev_set_default_storage(sd);
}
```

---

## 7. 多平台移植指南

### 7.1 移植步骤

| 步骤 | 操作 | 涉及文件 |
|------|------|----------|
| 1 | 创建平台目录 | `Port/新平台名/` |
| 2 | 实现port_system | `port_system.c` (时钟/延时/临界区) |
| 3 | 实现port_uart | `port_uart.c` (串口操作) |
| 4 | 实现port_spi | `port_spi.c` (SPI操作) |
| 5 | 实现port_gpio | `port_gpio.c` (GPIO操作) |
| 6 | 配置编译选项 | 修改Makefile/工程文件 |
| 7 | 测试验证 | 运行测试用例 |

### 7.2 移植检查清单

```
[ ] port_system_init() - 系统时钟初始化
[ ] port_delay_ms() - 毫秒延时
[ ] port_delay_us() - 微秒延时
[ ] port_get_tick() - 系统滴答计数
[ ] port_enter_critical() / port_exit_critical() - 临界区

[ ] port_uart_init() - 串口初始化
[ ] port_uart_send() - 串口发送
[ ] port_uart_set_baudrate() - 波特率切换
[ ] port_uart_set_parity() - 校验位切换
[ ] UART RX中断处理

[ ] port_spi_init() - SPI初始化
[ ] port_spi_transfer() - SPI收发
[ ] port_spi_cs_low/high() - 片选控制

[ ] port_gpio_init() - GPIO初始化
[ ] port_gpio_write() - GPIO输出
[ ] port_gpio_read() - GPIO读取
```

### 7.3 条件编译配置

```c
/* Port/port_config.h */

#ifndef __PORT_CONFIG_H__
#define __PORT_CONFIG_H__

/* 选择目标平台（在Makefile或工程配置中定义） */
#if defined(PLATFORM_STM32G4)
    #include "STM32G4/port_stm32g4.h"
#elif defined(PLATFORM_STM32F1)
    #include "STM32F1/port_stm32f1.h"
#elif defined(PLATFORM_STM32F4)
    #include "STM32F4/port_stm32f4.h"
#else
    #error "Please define target platform!"
#endif

/* 默认设备名称配置 */
#ifndef ISP_DEFAULT_SERIAL
#define ISP_DEFAULT_SERIAL  "STC_UART"
#endif

#ifndef LOG_DEFAULT_SERIAL
#define LOG_DEFAULT_SERIAL  "LOG_UART"
#endif

#endif /* __PORT_CONFIG_H__ */
```

---

## 8. 性能优化设计

### 8.1 零拷贝数据传输

```c
/* 直接操作环形缓冲区的连续区域 */
typedef struct {
    uint8_t *ptr;
    uint16_t len;
} ring_chunk_t;

/* 获取连续可读区域（避免拷贝） */
uint16_t ringbuffer_get_read_chunk(ringbuffer_t *rb, ring_chunk_t *chunk);

/* 获取连续可写区域（DMA直接写入） */
uint16_t ringbuffer_get_write_chunk(ringbuffer_t *rb, ring_chunk_t *chunk);
```

### 8.2 DMA传输支持

```c
/* 串口DMA发送（非阻塞） */
dev_status_t serial_write_dma(serial_dev_t *dev, const uint8_t *data, uint16_t len,
                              void (*complete_cb)(void));

/* 显示批量刷新（DMA） */
dev_status_t display_fill_rect_dma(display_dev_t *dev, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h, const uint16_t *data);
```

### 8.3 内联关键路径

```c
/* 串口字节发送（内联优化） */
BSP_INLINE void port_uart_send_byte_fast(USART_TypeDef *uart, uint8_t data)
{
    while (!(uart->ISR & USART_ISR_TXE_TXFNF));
    uart->TDR = data;
}
```

---

## 9. 架构总结

### 9.1 核心优势

| 特性 | 实现方式 | 效益 |
|------|----------|------|
| **快速串口重定向** | 运行时设备切换 | 一行代码切换串口 |
| **多平台兼容** | Port层隔离硬件差异 | 新平台仅需实现Port层 |
| **统一设备模型** | 串口/显示/存储同构接口 | 降低学习成本 |
| **高度解耦** | 依赖注入 + 回调机制 | 模块独立可测 |
| **高执行效率** | LL库 + DMA + 内联 | 最小化运行时开销 |

### 9.2 文件依赖图

```
APP Layer
    │
    ├── Service Layer
    │       │
    │       ├── ISP Core ──────────┐
    │       │       │              │
    │       │       ▼              │
    │       │   Protocol ◀────── MCU Database
    │       │
    │       └── Log / RingBuffer / Config
    │
    ├── BSP Layer
    │       │
    │       ├── Device Interface (Abstract)
    │       │       │
    │       │       ▼
    │       └── Device Implementations
    │               │
    │               ▼
    └── Port Layer (Platform Specific)
            │
            └── Hardware Registers
```

### 9.3 下一步行动

1. **创建接口头文件**: `BSP/Interface/` 目录下的设备接口定义
2. **实现Port层**: `Port/STM32G4/` 下的平台移植代码
3. **重构现有BSP**: 按新架构调整 `bsp_serial`, `bsp_lcd`
4. **实现设备管理器**: `BSP/dev_manager.c`
5. **重构ISP核心**: 使用新的设备接口

---

**文档结束**

