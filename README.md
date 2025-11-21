# STC脱机烧录器工程

这是一个基于STM32G431的STC脱机烧录器项目。

## 项目简介

本项目使用STM32G431微控制器，通过HAL库开发，实现STC单片机的脱机烧录功能。

## 硬件平台

- MCU: STM32G431RBT6
- 开发环境: Keil MDK-ARM
- HAL库版本: STM32G4xx HAL Driver

## 项目结构

```
.
├── BSP/              # 板级支持包
│   ├── bsp_sdcard.c  # SD卡驱动
│   └── bsp_spi.c     # SPI驱动
├── Drivers/          # STM32 HAL驱动库
├── FatFS/            # FatFS文件系统
├── Inc/              # 头文件
├── MDK-ARM/          # Keil工程文件
├── Src/              # 源代码文件
└── HAL_06_LCD.ioc    # STM32CubeMX配置文件
```

## 主要功能

- LCD显示
- SPI通信
- SD卡支持
- FatFS文件系统

## 开发环境

- Keil MDK-ARM 5.x
- STM32CubeMX
- STM32CubeIDE (可选)

## 编译说明

1. 使用Keil MDK-ARM打开 `MDK-ARM/HAL_06_LCD.uvprojx`
2. 编译工程
3. 下载到目标板

## 许可证

本项目仅供学习和研究使用。

