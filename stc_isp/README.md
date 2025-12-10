# STC 单片机离线烧录器

**项目版本**: 1.0.0  
**创建日期**: 2025-12-10  
**目标平台**: STM32G4xx (STM32G431RBTx)  
**开发环境**: Keil MDK-ARM / LL库

---

## 目录

- [1. 项目概述](#1-项目概述)
- [2. 功能特性](#2-功能特性)
- [3. 硬件需求](#3-硬件需求)
- [4. 软件架构](#4-软件架构)
- [5. 模块设计](#5-模块设计)
- [6. 通信设计](#6-通信设计)
- [7. 开发计划](#7-开发计划)
- [8. 参考资料](#8-参考资料)

---

## 1. 项目概述

### 1.1 项目背景

STC系列单片机采用UART/USB引导加载程序（BSL）进行ISP烧录，官方仅提供Windows GUI工具。本项目旨在基于STM32G4xx平台开发一款离线烧录器，实现对STC 89/10/11/12/15/8系列单片机的独立编程功能。

### 1.2 项目目标

| 目标 | 说明 |
|------|------|
| **离线烧录** | 无需PC，独立完成STC单片机烧录 |
| **多系列支持** | 支持STC89至STC8全系列 (1140+型号) |
| **自动识别** | 自动检测目标MCU型号 |
| **灵活配置** | 支持设备选项配置 |
| **用户友好** | LCD显示 + 按键操作界面 |

### 1.3 数据来源

MCU型号数据库提取自开源项目 [stcgal](https://github.com/grigorig/stcgal)，共包含 **1140** 个STC单片机型号信息。

---

## 2. 功能特性

### 2.1 核心功能

| 功能 | 描述 | 优先级 |
|------|------|--------|
| MCU型号识别 | 通过Magic码自动识别目标单片机 | P0 |
| Flash编程 | 烧录用户代码到Code Flash | P0 |
| EEPROM编程 | 烧录数据到IAP/EEPROM区 | P1 |
| 芯片擦除 | 全片或扇区擦除 | P0 |
| 数据校验 | 烧录后数据完整性校验 | P0 |
| 设备信息显示 | 显示型号、Flash大小、UID等 | P1 |

### 2.2 高级功能

| 功能 | 描述 | 优先级 |
|------|------|--------|
| RC振荡器校准 | 内部时钟频率微调 (STC15/8) | P2 |
| 设备选项配置 | 配置熔丝位/选项字节 | P2 |
| 固件存储 | SD卡存储多个固件文件 | P1 |
| 工作频率检测 | 测量目标MCU实际工作频率 | P2 |
| 批量烧录 | 连续自动烧录模式 | P3 |

### 2.3 支持的STC系列

| 系列 | 架构 | 支持状态 | 备注 |
|------|------|----------|------|
| STC89 | 8051 | ✅ 计划支持 | 最早期系列 |
| STC10 | 8051 | ✅ 计划支持 | - |
| STC11 | 8051 | ✅ 计划支持 | - |
| STC12 | 8051 | ✅ 计划支持 | 需偶校验 |
| STC15 | 8051 | ✅ 计划支持 | 支持RC校准 |
| STC8 | 8051+ | ✅ 计划支持 | 功能最丰富 |
| STC32 | MCS-251 | ⏳ 待研究 | 不同架构 |

---

## 3. 硬件需求

### 3.1 主控平台

| 项目 | 规格 |
|------|------|
| MCU | STM32G431RBTx |
| 内核 | ARM Cortex-M4 |
| 主频 | 80MHz |
| Flash | 128KB |
| SRAM | 32KB |

### 3.2 外设配置

| 外设 | 用途 | 配置 |
|------|------|------|
| USART2 | STC目标通信 | 可变波特率，支持偶校验切换 |
| USART1 | 调试日志输出 | 115200, 8N1 |
| SPI1 | SD卡通信 | 用于固件存储 |
| GPIO | 目标复位控制 | 硬件复位STC目标 |
| LCD | 状态显示 | TFT/OLED显示屏 |
| KEY | 用户交互 | 按键输入 |

### 3.3 接口定义

| 接口 | 引脚 | 说明 |
|------|------|------|
| STC_TX | PA2 (USART2_TX) | 发送至STC目标 |
| STC_RX | PA3 (USART2_RX) | 接收自STC目标 |
| STC_RST | 待定 | STC目标复位控制 |
| STC_VCC | 待定 | STC目标供电控制 (可选) |

---

## 4. 软件架构

### 4.1 分层架构

本项目采用改进的5层架构，引入Port层实现多平台兼容：

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

### 4.2 当前代码状态

> **注意**: 当前代码仍在 `stc_isp/` 目录下，尚未完全迁移到目标架构。
> 后续开发将按下方目标目录结构进行重构。

**当前文件位置**:
```
stc_isp/                         # 临时位置，待重构
├── stc_mcu_database.c/h        # → Middlewares/stc_isp/
├── stc_hal.c/h                 # → 移除，由Port层+设备接口替代
├── stc_isp_protocol.c/h        # → Service/isp_core/
└── README.md
```

### 4.3 目标目录结构

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
│   │   └── dev_common.h        # 公共定义
│   ├── Serial/                  # 串口设备实现
│   │   ├── bsp_serial.c/h      # 串口设备管理器
│   │   └── bsp_serial_stc.c/h  # STC专用串口封装
│   ├── Display/                 # 显示设备实现
│   │   └── bsp_lcd_st7735.c/h  # ST7735 LCD驱动
│   ├── Storage/                 # 存储设备实现
│   │   └── bsp_sdcard.c/h      # SD卡驱动
│   └── Key/                     # 按键设备
│       └── bsp_key.c/h
│
├── Port/                         # L0 平台移植层
│   ├── port_def.h               # 移植层公共定义
│   ├── STM32G4/                 # STM32G4平台
│   │   ├── port_uart.c/h       # UART移植
│   │   ├── port_spi.c/h        # SPI移植
│   │   ├── port_gpio.c/h       # GPIO移植
│   │   └── port_system.c/h     # 系统移植
│   └── STM32F1/                 # STM32F1平台(预留)
│
├── Inc/                          # 全局头文件
└── Src/                          # CubeMX生成代码
```

### 4.4 依赖关系

```
APP Layer
    │
    ├── Service Layer
    │       │
    │       ├── ISP Core ──────────┐
    │       │       │              │
    │       │       ▼              │
    │       │   Protocol ◀────── MCU Database (Middlewares)
    │       │
    │       └── Log / RingBuffer / Config
    │
    ├── BSP Layer
    │       │
    │       ├── Device Interface (Abstract)
    │       │       │
    │       │       ▼
    │       └── Device Implementations (Serial/Display/Storage)
    │               │
    │               ▼
    └── Port Layer (Platform Specific)
            │
            └── Hardware Registers (LL库)
```

---

## 5. 模块设计

### 5.1 已完成模块

| 模块 | 当前位置 | 目标位置 | 状态 | 说明 |
|------|----------|----------|------|------|
| MCU数据库 | `stc_isp/stc_mcu_database.c/h` | `Middlewares/stc_isp/` | ✅ 完成 | 1140个型号数据 |
| HAL抽象 | `stc_isp/stc_hal.c/h` | 移除 (Port层替代) | ⚠️ 待重构 | 将迁移到Port层 |
| ISP协议 | `stc_isp/stc_isp_protocol.c/h` | `Service/isp_core/` | 🔄 框架完成 | 待迁移并完善 |
| 环形缓冲区 | 待确认 | `Service/ringbuffer.c/h` | ✅ 完成 | 面向对象风格 |
| 日志服务 | 待确认 | `Service/log.c/h` | ✅ 完成 | 调试输出 |

### 5.2 待开发模块

| 模块 | 文件位置 | 优先级 | 说明 |
|------|----------|--------|------|
| 设备接口定义 | `BSP/Interface/dev_*.h` | P0 | 统一设备抽象接口 |
| Port层-UART | `Port/STM32G4/port_uart.c/h` | P0 | UART LL库移植 |
| Port层-系统 | `Port/STM32G4/port_system.c/h` | P0 | 时钟/延时/临界区 |
| 串口设备 | `BSP/Serial/bsp_serial_stc.c/h` | P0 | STC专用串口封装 |
| ISP协议 | `Service/isp_core/stc_isp_protocol.c/h` | P0 | 握手/擦除/编程/校验 |
| ISP状态机 | `Service/isp_core/stc_isp_core.c/h` | P0 | 烧录流程控制 |
| 协议定义 | `Middlewares/stc_isp/stc_protocol_def.h` | P1 | 协议帧结构定义 |

### 5.3 MCU数据库说明

数据库包含每个STC型号的关键信息：

| 字段 | 类型 | 说明 |
|------|------|------|
| name | string | 型号名称 |
| magic | uint16_t | 型号识别码 |
| total_flash | uint32_t | 总Flash大小 |
| code_flash | uint32_t | 代码区大小 |
| eeprom_flash | uint32_t | EEPROM大小 |
| iap_support | bool | IAP支持 |
| calibrate_support | bool | RC校准支持 |
| is_mcs251 | bool | MCS-251架构标识 |

---

## 6. 通信设计

### 6.1 串口配置

| 参数 | 值 | 说明 |
|------|------|------|
| 外设 | USART2 | 与STC目标通信 |
| 库 | LL库 | 轻量级底层库 |
| 接收方式 | 中断接收 | RXNE中断 |
| 数据处理 | 主循环处理 | 非阻塞架构 |
| 缓冲区 | 环形缓冲区 | 面向对象风格 |

### 6.2 数据流架构

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│   USART2    │────▶│  环形缓冲区   │────▶│  协议解析   │
│  RXNE中断   │     │  (ringbuffer) │     │  (主循环)   │
└─────────────┘     └──────────────┘     └─────────────┘
                           ▲
                           │
                    ┌──────┴──────┐
                    │  ISR写入     │
                    │  主循环读取   │
                    └─────────────┘
```

### 6.3 波特率策略

| 阶段 | 波特率 | 说明 |
|------|--------|------|
| 握手检测 | 2400 | 低速发送，兼容性最好 |
| 握手应答 | 自适应 | 根据MCU响应调整 |
| 编程传输 | 协商 | 可达115200-230400 |

### 6.4 串口校验配置

| STC系列 | 数据位 | 校验位 | 停止位 |
|---------|--------|--------|--------|
| STC89/10/11 | 8 | 无 | 1 |
| STC12/15/8 | 8 | 偶校验 | 1 |

---

## 7. 开发计划

### 7.1 里程碑

| 阶段 | 目标 | 状态 |
|------|------|------|
| M1 | 基础框架搭建 | ✅ 完成 |
| M1.5 | 代码重构至新架构 | ⏳ 待开始 |
| M2 | Port层与串口设备实现 | 🔄 进行中 |
| M3 | 握手协议实现 | ⏳ 待开始 |
| M4 | 擦除/编程实现 | ⏳ 待开始 |
| M5 | 校验/配置实现 | ⏳ 待开始 |
| M6 | UI与SD卡集成 | ⏳ 待开始 |

### 7.2 M2阶段详细任务

| 任务 | 描述 | 优先级 |
|------|------|--------|
| Port层系统初始化 | 时钟/延时/临界区实现 | P0 |
| Port层UART移植 | USART2 LL库初始化/收发 | P0 |
| 设备接口定义 | 创建统一设备抽象接口 | P0 |
| 串口设备实现 | 基于Port层实现串口设备 | P0 |
| 波特率切换 | 动态波特率配置 | P0 |
| 校验位切换 | 8N1/8E1切换 | P1 |
| 缓冲区集成 | 环形缓冲区对接中断接收 | P0 |

### 7.3 文件创建计划

```
阶段2 - 基础架构与串口通信:
  ├── Port/port_def.h                       # 移植层公共定义
  ├── Port/STM32G4/port_system.c/h          # 系统移植
  ├── Port/STM32G4/port_uart.c/h            # UART移植
  ├── BSP/Interface/dev_common.h            # 设备公共定义
  ├── BSP/Interface/dev_serial.h            # 串口设备接口
  └── BSP/Serial/bsp_serial_stc.c/h         # STC专用串口封装

阶段3 - 握手协议:
  ├── Middlewares/stc_isp/stc_protocol_def.h    # 协议帧结构定义
  └── Service/isp_core/stc_isp_protocol.c/h     # 协议实现

阶段4 - ISP核心:
  └── Service/isp_core/stc_isp_core.c/h     # 烧录状态机

阶段5 - 应用层:
  ├── BSP/dev_manager.c/h                   # 设备管理器
  └── APP/app_isp.c/h                       # 烧录应用
```

---

## 8. 参考资料

### 8.1 开源项目

| 项目 | 地址 | 说明 |
|------|------|------|
| stcgal | https://github.com/grigorig/stcgal | Python ISP工具 |
| stc8prog | https://github.com/stawel/stc8prog | STC8专用工具 |

### 8.2 项目文档

| 文档 | 位置 | 说明 |
|------|------|------|
| 协议分析 | `docs/STC_ISP_Protocol_Analysis.md` | 响应数据分析 |
| 握手流程 | `docs/STC_STM32_handshake_to_flash.md` | 握手到烧录流程 |
| 离线算法 | `docs/STC8_offline_algorithm.md` | STC8离线烧录 |

### 8.3 协议要点

- STC ISP协议为私有协议，非公开文档
- 不同系列协议存在差异，需分别适配
- 响应数据结构固定，内容部分可变
- Magic码用于识别MCU型号和选择协议配置

---

## 附录

### A. 命名规范

| 类型 | 前缀 | 示例 |
|------|------|------|
| Port层函数 | `port_` | `port_uart_init()` |
| BSP层函数 | `bsp_` | `bsp_serial_create()` |
| 设备操作 | 结构体函数指针 | `dev->write()` |
| Service层函数 | `svc_` / `isp_` | `isp_start_handshake()` |
| 协议函数 | `stc_` | `stc_parse_mcu_info()` |
| APP层函数 | `app_` | `app_isp_flash_firmware()` |

### B. 错误码定义

| 错误码 | 值 | 说明 |
|--------|------|------|
| STC_OK | 0 | 操作成功 |
| STC_ERR_TIMEOUT | -1 | 操作超时 |
| STC_ERR_COMM | -2 | 通信错误 |
| STC_ERR_CHECKSUM | -3 | 校验和错误 |
| STC_ERR_UNSUPPORTED | -4 | 不支持的型号 |
| STC_ERR_VERIFY | -5 | 校验失败 |

---

**文档结束**

