# 日志服务层使用说明

## 概述

日志服务层提供了灵活的日志输出功能，支持：
- 注册式输出接口（可注册多个输出方式）
- 两种输出类型：printf格式和非printf格式（原始数据）
- 日志级别控制（DEBUG、INFO、WARN、ERROR）
- 条件编译控制（printf重定向、时间戳等）
- 格式化输出（类似printf）

## 文件说明

- `log.h` / `log.c`: 日志服务核心实现
- `log_uart_adapter.h` / `log_uart_adapter.c`: UART输出适配器示例

## 配置选项

在`log.h`中可以通过条件编译控制功能：

```c
// 启用printf重定向支持（需要重定向printf到日志系统）
#define LOG_ENABLE_PRINTF_REDIRECT  1

// 启用时间戳输出
#define LOG_ENABLE_TIMESTAMP        1

// 最大输出句柄数量
#define LOG_MAX_OUTPUT_HANDLES      4

// 日志消息缓冲区大小（用于非printf格式输出）
#define LOG_BUFFER_SIZE             256
```

## 快速开始

### 1. 初始化日志服务

#### 方式1：原始数据输出（需要先sprintf格式化）

适用于需要先格式化再输出的场景（如DMA传输、某些硬件接口等）：

```c
#include "log.h"
#include "log_uart_adapter.h"

int main(void)
{
    // 初始化HAL库和系统时钟
    HAL_Init();
    SystemClock_Config();
    
    // 初始化UART（如果使用UART输出）
    MX_USART1_UART_Init();
    
    // 初始化日志服务
    log_init();
    
    // 注册UART输出适配器（原始数据输出方式）
    log_uart_adapter_init();
    
    // 设置日志级别（可选，默认为DEBUG）
    log_set_level(LOG_LEVEL_DEBUG);
    
    // 现在可以使用日志了
    LOG_INFO("系统初始化完成");
    
    while (1)
    {
        // 主循环
    }
}
```

#### 方式2：printf格式输出（直接格式化输出）

适用于可以直接使用printf/vprintf的场景：

```c
#include "log.h"
#include "log_uart_adapter.h"

int main(void)
{
    // 初始化HAL库和系统时钟
    HAL_Init();
    SystemClock_Config();
    
    // 初始化UART（如果使用UART输出）
    MX_USART1_UART_Init();
    
    // 初始化日志服务
    log_init();
    
    // 注册UART输出适配器（printf格式输出方式）
    log_uart_adapter_init_printf();
    
    // 设置日志级别（可选，默认为DEBUG）
    log_set_level(LOG_LEVEL_DEBUG);
    
    // 现在可以使用日志了
    LOG_INFO("系统初始化完成");
    
    while (1)
    {
        // 主循环
    }
}
```

### 2. 使用日志宏

```c
// DEBUG级别日志
LOG_DEBUG("调试信息: 变量值 = %d", value);

// INFO级别日志
LOG_INFO("系统状态: %s", status);

// WARN级别日志
LOG_WARN("警告: 温度过高，当前温度 = %d°C", temperature);

// ERROR级别日志
LOG_ERROR("错误: 初始化失败，错误码 = %d", error_code);
```

### 3. 自定义输出接口

如果需要使用其他输出方式（如SWO、LCD等），可以自定义输出函数。支持两种方式：

#### 方式1：原始数据输出（需要先sprintf格式化）

适用于需要先格式化再输出的场景：

```c
// 自定义原始数据输出函数
int my_custom_output_raw(const uint8_t *data, uint32_t len)
{
    // 实现自定义输出逻辑
    for (uint32_t i = 0; i < len; i++)
    {
        // 输出到自定义设备（数据已经格式化好）
        custom_device_write(data[i]);
    }
    return 0;
}

// 注册自定义输出（原始数据输出方式）
log_output_handle_t my_handle;
my_handle.output_type = LOG_OUTPUT_TYPE_RAW;
my_handle.output_func.raw_func = my_custom_output_raw;
my_handle.user_data = NULL;
log_register_output(&my_handle);
```

#### 方式2：printf格式输出（直接格式化输出）

适用于可以直接使用printf/vprintf的场景：

```c
#include <stdarg.h>
#include <stdio.h>

// 自定义printf格式输出函数
int my_custom_output_printf(const char *format, va_list args)
{
    // 直接使用vprintf或自定义格式化逻辑
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    
    // 输出到自定义设备
    for (int i = 0; i < len; i++)
    {
        custom_device_write(buffer[i]);
    }
    return 0;
}

// 注册自定义输出（printf格式输出方式）
log_output_handle_t my_handle;
my_handle.output_type = LOG_OUTPUT_TYPE_PRINTF;
my_handle.output_func.printf_func = my_custom_output_printf;
my_handle.user_data = NULL;
log_register_output_printf(&my_handle);
```

### 4. 日志级别控制

```c
// 设置日志级别
log_set_level(LOG_LEVEL_INFO);  // 只输出INFO、WARN、ERROR

// 获取当前日志级别
log_level_t level = log_get_level();
```

日志级别说明：
- `LOG_LEVEL_DEBUG`: 输出所有日志（DEBUG、INFO、WARN、ERROR）
- `LOG_LEVEL_INFO`: 只输出INFO、WARN、ERROR
- `LOG_LEVEL_WARN`: 只输出WARN、ERROR
- `LOG_LEVEL_ERROR`: 只输出ERROR

### 5. printf重定向（可选）

如果启用了`LOG_ENABLE_PRINTF_REDIRECT`，可以在`syscalls.c`中重定向printf：

```c
#include "log.h"

int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)len;
    
    // 重定向到日志系统
    char buffer[256];
    strncpy(buffer, ptr, len);
    buffer[len] = '\0';
    log_printf_redirect("%s", buffer);
    
    return len;
}
```

## API参考

### 初始化函数

```c
void log_init(void);
```
初始化日志服务，清空所有注册的输出接口。

### 输出接口管理

```c
int log_register_output(log_output_handle_t *handle);
int log_register_output_printf(log_output_handle_t *handle);
int log_unregister_output(log_output_handle_t *handle);
```
注册/注销日志输出接口。
- `log_register_output`: 注册原始数据输出接口（需要先sprintf格式化）
- `log_register_output_printf`: 注册printf格式输出接口（直接格式化输出）

### 日志级别控制

```c
void log_set_level(log_level_t level);
log_level_t log_get_level(void);
```
设置/获取日志级别。

### 日志输出函数

```c
void log_write(log_level_t level, const char *format, ...);
```
直接输出日志（通常使用宏更方便）。

### 便捷宏

```c
LOG_DEBUG(fmt, ...)
LOG_INFO(fmt, ...)
LOG_WARN(fmt, ...)
LOG_ERROR(fmt, ...)
```
便捷的日志输出宏，自动添加日志级别前缀。

## 输出格式

日志输出格式为：
```
[时间戳] [级别] 用户消息\r\n
```

例如：
```
[00001234] [DEBUG] 调试信息: 变量值 = 123
[00001235] [INFO ] 系统初始化完成
[00001236] [WARN ] 警告: 温度过高，当前温度 = 85°C
[00001237] [ERROR] 错误: 初始化失败，错误码 = -1
```

如果禁用了时间戳（`LOG_ENABLE_TIMESTAMP = 0`），格式为：
```
[级别] 用户消息\r\n
```

## 两种输出方式的权衡

### 原始数据输出（LOG_OUTPUT_TYPE_RAW）
- **优点**：
  - 适用于DMA传输、某些硬件接口等需要先格式化再输出的场景
  - 可以精确控制输出时机
- **缺点**：
  - 需要额外的缓冲区（LOG_BUFFER_SIZE）
  - 需要先sprintf格式化，增加一次内存拷贝

### printf格式输出（LOG_OUTPUT_TYPE_PRINTF）
- **优点**：
  - 直接格式化输出，减少内存占用
  - 适用于可以直接使用printf/vprintf的场景
- **缺点**：
  - 需要输出接口支持va_list参数
  - 某些硬件接口可能不支持

## 注意事项

1. 日志缓冲区大小为256字节（可通过`LOG_BUFFER_SIZE`修改），超过部分会被截断
2. 最多支持注册4个输出接口（可通过`LOG_MAX_OUTPUT_HANDLES`修改）
3. 输出函数应该是阻塞式的，确保数据完整输出
4. 在中断中使用日志时需要注意输出函数的实现（避免阻塞）
5. Release模式下可以通过定义`RELEASE_BUILD`来禁用DEBUG日志

## 示例代码

完整示例请参考 `log_uart_adapter.c` 中的实现。
