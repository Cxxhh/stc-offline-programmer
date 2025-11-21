# BSP和FatFS集成说明

## 概述
本文档说明如何在STM32G4xx工程中使用BSP层SD卡驱动和FatFS文件系统。

## 已完成的移植工作

### 1. 硬件适配
- ✅ 将STM32F1xx库替换为STM32G4xx库
- ✅ DMA通道适配：从Channel 2/3改为Channel 1/2
- ✅ 移除FLASH_CS引脚依赖，支持单独使用SD卡
- ✅ 使用HAL_GPIO_WritePin代替直接寄存器操作

### 2. 引脚配置
当前工程使用的SD卡引脚：
```
SPI1_SCK  : PA5
SPI1_MISO : PA6
SPI1_MOSI : PA7
SD_CS     : PB10
RGB_LED   : PB11 (可选的状态指示灯)
```

### 3. DMA配置
```
DMA1_Channel1 : SPI1_RX
DMA1_Channel2 : SPI1_TX
```

## 文件结构

```
BSP/
├── bsp_common.h        # 公共定义和类型
├── bsp_spi.h/c         # SPI DMA总线管理器
├── bsp_sdcard.h/c      # SD卡驱动（SPI模式）

FatFS/
└── src/
    ├── ff.h/c          # FatFS核心
    ├── diskio.h/c      # 磁盘I/O接口
    └── diskio_sdcard.c # SD卡到FatFS的适配层
```

## 使用方法

### 1. 在你的代码中包含头文件
```c
#include "../BSP/bsp_sdcard.h"
#include "../FatFS/src/ff.h"
```

### 2. 初始化SD卡
```c
// 初始化SD卡硬件
if (bsp_sdcard_init() == BSP_SDCARD_OK) {
    // SD卡初始化成功
}

// 获取SD卡信息
bsp_sdcard_info_t sd_info;
bsp_sdcard_detect(&sd_info);
```

### 3. 挂载文件系统
```c
FATFS fs;
FRESULT res = f_mount(&fs, "0:", 1);
if (res == FR_OK) {
    // 文件系统挂载成功
}
```

### 4. 文件操作示例

#### 写入文件
```c
FIL file;
UINT bw;

res = f_open(&file, "0:test.txt", FA_CREATE_ALWAYS | FA_WRITE);
if (res == FR_OK) {
    const char* data = "Hello from STM32G4!";
    f_write(&file, data, strlen(data), &bw);
    f_close(&file);
}
```

#### 读取文件
```c
FIL file;
UINT br;
char buffer[128];

res = f_open(&file, "0:test.txt", FA_READ);
if (res == FR_OK) {
    f_read(&file, buffer, sizeof(buffer), &br);
    f_close(&file);
}
```

#### 列出目录
```c
DIR dir;
FILINFO fno;

res = f_opendir(&dir, "0:");
if (res == FR_OK) {
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;
        // 处理文件信息
    }
    f_closedir(&dir);
}
```

## 中断处理

已在`stm32g4xx_it.c`中添加DMA中断处理：

```c
void DMA1_Channel1_IRQHandler(void) {
    // SPI1 RX DMA中断
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        bsp_spi_dma_rx_complete_callback();
    }
    // 错误处理...
}

void DMA1_Channel2_IRQHandler(void) {
    // SPI1 TX DMA中断
    if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
        LL_DMA_ClearFlag_TC2(DMA1);
        bsp_spi_dma_tx_complete_callback();
    }
    // 错误处理...
}
```

## 注意事项

### 1. SPI速度配置
当前SPI速度设置为`DIV2`（最高速度），如果遇到稳定性问题，可以在`spi.c`中降低速度：
```c
SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV4;  // 或更慢
```

### 2. 如果有其他SPI设备
如果你的工程中还有其他SPI设备（如W25Qxx Flash），需要在`bsp_sdcard.c`的`bsp_sdcard_select()`函数中添加：
```c
void bsp_sdcard_select(void) {
    // 取消选择其他SPI设备
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    
    // 选中SD卡
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}
```

### 3. DMA总线管理
BSP提供了DMA总线锁机制，如果多个SPI设备共享同一个SPI+DMA：
```c
if (bsp_spi_dma_lock(BSP_SPI_DMA_DEVICE_SD_CARD)) {
    // 已锁定DMA，可以进行传输
    bsp_spi_dma_transmit_receive(tx_buf, rx_buf, len, timeout);
    bsp_spi_dma_unlock(BSP_SPI_DMA_DEVICE_SD_CARD);
}
```

### 4. 卡检测
提供两种检测方式：
- `bsp_sdcard_is_inserted()` - 检查是否已初始化
- `bsp_sdcard_detect_fast()` - 发送CMD0快速探测
- `bsp_sdcard_check_alive()` - 发送CMD13心跳检测

## 编译配置

### Keil MDK-ARM
在工程设置中添加以下包含路径：
```
../BSP
../FatFS/src
```

确保以下文件被添加到工程：
- `BSP/bsp_spi.c`
- `BSP/bsp_sdcard.c`
- `FatFS/src/ff.c`
- `FatFS/src/diskio.c`
- `FatFS/src/diskio_sdcard.c`

## 测试代码

已在`main.c`中添加了完整的测试代码，包括：
1. SD卡初始化
2. 显示SD卡类型
3. 挂载FatFS
4. 写入测试文件
5. 读取测试文件
6. LCD显示测试结果

## 故障排除

### 问题1：SD卡初始化失败
- 检查SPI接线是否正确
- 确认CS引脚配置为推挽输出
- 尝试降低SPI速度
- 确认SD卡已正确插入

### 问题2：文件系统挂载失败
- 确认SD卡已格式化为FAT32
- 检查`ffconf.h`配置
- 确认扇区读取功能正常

### 问题3：DMA传输不工作
- 检查DMA中断是否使能
- 确认回调函数已正确调用
- 检查DMA通道配置是否正确

## 版本信息
- BSP版本: V2.0.0
- 适配芯片: STM32G4xx系列
- 迁移日期: 2025-01

