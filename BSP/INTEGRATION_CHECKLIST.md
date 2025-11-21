# BSP和FatFS集成检查清单

## 📋 需要添加到Keil工程的文件

### BSP文件
- [ ] `BSP/bsp_spi.c`
- [ ] `BSP/bsp_sdcard.c`

### FatFS文件
- [ ] `FatFS/src/ff.c`
- [ ] `FatFS/src/diskio.c`
- [ ] `FatFS/src/diskio_sdcard.c`

## 🔧 需要修改的文件

### 已自动完成
- ✅ `BSP/bsp_common.h` - 已适配STM32G4xx
- ✅ `BSP/bsp_spi.c/h` - 已适配DMA Channel 1/2
- ✅ `BSP/bsp_sdcard.c/h` - 已移除FLASH_CS依赖
- ✅ `Src/stm32g4xx_it.c` - 已添加DMA中断处理
- ✅ `Src/main.c` - 已添加SD卡测试代码
- ✅ `Src/spi.c` - 已修改为8位数据宽度并使能SPI

## 🎯 Keil工程配置

### 1. 添加头文件包含路径
在工程选项 → C/C++ → Include Paths 中添加：
```
..\BSP
..\FatFS\src
```

### 2. 确认外设已配置
- [x] SPI1已初始化（PA5-SCK, PA6-MISO, PA7-MOSI）
- [x] DMA1已初始化（Channel 1和2）
- [x] PB10已配置为GPIO输出（SD_CS）
- [x] PB11已配置为GPIO输出（RGB LED，可选）

### 3. 确认中断优先级
- [x] DMA1_Channel1_IRQn已使能
- [x] DMA1_Channel2_IRQn已使能

## 🧪 测试步骤

### 1. 硬件准备
- [ ] 将SD卡插入卡槽
- [ ] 确认SD卡已格式化为FAT32
- [ ] 检查SPI接线是否正确

### 2. 编译工程
- [ ] 添加所有必需的源文件
- [ ] 设置正确的包含路径
- [ ] 编译通过，无错误

### 3. 烧录测试
- [ ] 烧录程序到MCU
- [ ] 观察LCD显示
- [ ] 确认"SD Init: OK"
- [ ] 确认"FatFS: Mounted"
- [ ] 确认"Write: OK"
- [ ] 确认"Read: OK"

### 4. 验证结果
- [ ] 将SD卡插入电脑
- [ ] 确认存在`test.txt`文件
- [ ] 打开文件，内容应为："Hello from STM32G4!\r\nSD Card works!\r\n"

## ⚠️ 常见问题

### SD卡初始化失败
原因可能是：
1. SPI速度太快 → 在`spi.c`中降低`BaudRate`为`DIV4`或`DIV8`
2. CS引脚未正确初始化 → 检查CubeMX配置
3. SD卡损坏或未格式化 → 更换SD卡或重新格式化

### 文件系统挂载失败
原因可能是：
1. SD卡未格式化为FAT32 → 使用电脑格式化为FAT32
2. 扇区读取失败 → 检查DMA中断是否正常工作
3. `ffconf.h`配置错误 → 使用提供的默认配置

### DMA不工作
原因可能是：
1. DMA中断未使能 → 检查`dma.c`中的NVIC配置
2. 中断回调未调用 → 检查`stm32g4xx_it.c`中的代码
3. SPI未使能 → 确认`LL_SPI_Enable(SPI1)`已执行

## 📝 下一步

集成完成后，你可以：
1. 实现日志记录功能
2. 保存传感器数据到SD卡
3. 从SD卡加载配置文件
4. 实现固件升级（IAP）功能

## 📞 技术支持

如有问题，请检查：
1. `BSP/README_BSP_INTEGRATION.md` - 详细使用说明
2. 示例代码在 `Src/main.c` 的 `USER CODE BEGIN 2` 区域
3. BSP层接口定义在各个头文件中

