# STC ISP 烧录器库

基于函数指针和配置表混合架构的STC多型号烧录器实现。

## 特性

- 支持所有STC主流型号（STC89/90、STC12、STC15、STC8、STC32）
- 支持自动识别和手动选择两种模式
- 模块化设计，易于扩展
- 适用于STM32等嵌入式平台

## 文件结构

```
stc_isp/
├── stc_isp.h               # 主头文件（包含所有模块）
├── stc_types.h             # 基础类型定义
├── stc_protocol_config.h   # 协议配置表
├── stc_protocol_ops.h      # 协议操作接口
├── stc_context.h/c         # 运行时上下文
├── stc_packet.h/c          # 数据包构建/解析
├── stc_model_db.h/c        # 型号数据库
├── stc_programmer.h/c      # 主控流程
├── protocols/
│   ├── stc89_protocol.h/c  # STC89/89A协议
│   ├── stc12_protocol.h/c  # STC12协议
│   ├── stc15_protocol.h/c  # STC15/15A协议
│   ├── stc8_protocol.h/c   # STC8/8d/8g/32协议
│   └── usb15_protocol.h/c  # USB协议（存根）
└── hal/
    └── stc_hal_stm32.h/c   # STM32 HAL实现
```

## 支持的协议

| 协议ID | 名称 | 支持型号 |
|--------|------|----------|
| STC_PROTO_STC89 | STC89/90系列 | STC89C52RC等 |
| STC_PROTO_STC89A | STC89A系列 | STC12C5052等 |
| STC_PROTO_STC12 | STC12系列 | STC12C5A60S2等 |
| STC_PROTO_STC15A | STC15A系列 | STC15F104E等 |
| STC_PROTO_STC15 | STC15系列 | STC15W4K64S4等 |
| STC_PROTO_STC8 | STC8系列 | STC8A8K64S4A12等 |
| STC_PROTO_STC8D | STC8H系列 | STC8H3K64S4等 |
| STC_PROTO_STC8G | STC8H1K系列 | STC8H1K08等 |
| STC_PROTO_STC32 | STC32系列 | STC32G12K128等 |

## 使用示例

### 1. 初始化

```c
#include "stc_isp.h"

// 初始化UART
stc_stm32_uart_t uart;
stc_hal_stm32_uart_init(&uart, &huart1);

// 初始化烧录器
stc_context_t ctx;
stc_programmer_init(&ctx, stc_hal_stm32_get(), &uart);
```

### 2. 自动识别模式

```c
// 设置自动识别模式
stc_set_mode_auto(&ctx);

// 连接MCU（提示用户给MCU上电）
int ret = stc_connect(&ctx, 30000);  // 30秒超时

if (ret == STC_OK) {
    // 获取MCU信息
    const stc_mcu_info_t* info = stc_get_mcu_info(&ctx);
    printf("检测到: %s\n", info->model_name);
    printf("Flash: %lu KB\n", info->flash_size / 1024);
    
    // 选择协议
    stc_select_protocol(&ctx);
    
    // 烧录
    ret = stc_program(&ctx, firmware_data, firmware_len, NULL);
} else if (ret == STC_ERR_UNKNOWN_MODEL) {
    printf("未知型号，请手动选择协议\n");
}
```

### 3. 手动选择模式

```c
// 设置手动模式，指定STC8H协议
stc_set_mode_manual(&ctx, STC_PROTO_STC8D);

// 连接
int ret = stc_connect(&ctx, 30000);

if (ret == STC_OK) {
    // 烧录
    ret = stc_program(&ctx, firmware_data, firmware_len, NULL);
}
```

### 4. 获取协议列表（用于UI）

```c
const char** names;
int count;
stc_get_protocol_list(&names, &count);

for (int i = 0; i < count; i++) {
    printf("%d: %s\n", i, names[i]);
}
```

### 5. 进度回调

```c
void progress_callback(uint32_t current, uint32_t total, void* user_data)
{
    int percent = (current * 100) / total;
    printf("烧录进度: %d%%\n", percent);
}

// 设置回调
stc_context_set_progress_callback(&ctx, progress_callback, NULL);
```

## 移植指南

### 1. 实现HAL接口

需要实现以下函数：

```c
typedef struct {
    int (*set_baudrate)(void* handle, uint32_t baudrate);
    int (*set_parity)(void* handle, stc_parity_t parity);
    int (*write)(void* handle, const uint8_t* data, uint16_t len, uint32_t timeout_ms);
    int (*read)(void* handle, uint8_t* data, uint16_t max_len, uint32_t timeout_ms);
    void (*flush)(void* handle);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);
} stc_hal_t;
```

### 2. 添加编译定义

```c
// 在编译选项中添加
#define STM32G4xx           // 或其他STM32系列
#define STM32_PLATFORM      // 启用STM32 HAL
```

### 3. 内存需求

- Flash: ~6KB（含型号数据库）
- RAM: ~1KB（上下文+缓冲区）

## 错误处理

```c
int ret = stc_program(&ctx, data, len, NULL);
if (ret != STC_OK) {
    printf("错误: %s\n", stc_get_error_string(ret));
}
```

错误码：
- `STC_OK` (0): 成功
- `STC_ERR_TIMEOUT`: 超时
- `STC_ERR_CHECKSUM`: 校验和错误
- `STC_ERR_FRAME`: 帧格式错误
- `STC_ERR_UNKNOWN_MODEL`: 未知型号
- `STC_ERR_ERASE_FAIL`: 擦除失败
- `STC_ERR_PROGRAM_FAIL`: 编程失败
- `STC_ERR_HANDSHAKE_FAIL`: 握手失败
- `STC_ERR_CALIBRATION_FAIL`: 校准失败

## 许可证

MIT License

