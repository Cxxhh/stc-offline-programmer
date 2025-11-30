# STC ISP烧录协议响应数据分析报告

**文档版本**: 1.0  
**创建日期**: 2025-11-30  
**作者**: STC ISP项目组  
**项目**: STC离线烧录器

---

## 目录

- [1. 概述](#1-概述)
- [2. 响应数据特征分析](#2-响应数据特征分析)
- [3. 数据包长度差异](#3-数据包长度差异)
- [4. 各阶段响应详解](#4-各阶段响应详解)
- [5. 编程实现建议](#5-编程实现建议)
- [6. 协议配置参考](#6-协议配置参考)
- [7. 注意事项](#7-注意事项)

---

## 1. 概述

### 1.1 问题背景

在STC单片机ISP烧录协议实现过程中，需要明确以下关键问题：

1. **每个阶段STC的回应数据是否固定？**
2. **不同型号在各阶段接收的数据长度是否相同？**

### 1.2 核心结论

✅ **响应数据结构固定，但内容部分可变**  
✅ **不同系列的数据包长度存在显著差异**  
✅ **需要根据MCU型号动态选择协议配置**

---

## 2. 响应数据特征分析

### 2.1 固定部分

STC响应数据中以下部分是固定的：

| 特征 | 说明 |
|------|------|
| **数据包长度** | 每个阶段响应的包长度在同一型号/系列中固定 |
| **标志字节位置** | 特定功能的标志字节位置固定 |
| **校验和位置** | 校验和通常在数据包末尾固定位置 |
| **协议头部** | 包含固定的起始标识 |

### 2.2 可变部分

以下内容会根据实际情况变化：

| 特征 | 说明 | 示例 |
|------|------|------|
| **MCU型号信息** | Magic码标识不同型号 | `0xF781`, `0xD164` 等 |
| **版本信息** | Bootloader版本号 | `7.1Q`, `7.2.5Q` 等 |
| **时钟频率** | 实际测量的工作频率 | 根据晶振实时计算 |
| **UID/序列号** | 芯片唯一标识 | 每片芯片不同 |
| **状态标志** | 当前操作状态 | 成功/失败/进度 |

### 2.3 响应数据结构示例

```
典型响应包结构：
+--------+--------+----------+----------+----------+
| 帧头   | 长度   | 命令类型 | 数据负载 | 校验和   |
| 1-2B   | 1B     | 1B       | 变长     | 1-2B     |
+--------+--------+----------+----------+----------+
  固定     固定     固定       可变       固定位置
```

---

## 3. 数据包长度差异

### 3.1 不同系列对比

| 系列 | 架构 | 握手响应 | 编程包大小 | 响应长度 | 复杂度 |
|------|------|----------|------------|----------|--------|
| **STC89** | 8051 | ~46-50B | 128B | 短(1-4B) | 简单 |
| **STC12** | 8051 | ~50-58B | 128B | 中(4-8B) | 中等 |
| **STC15** | 8051 | ~52-64B | 128B | 长(8-16B) | 复杂 |
| **STC8** | 8051+ | ~64-74B | 256B | 最长(16B+) | 最复杂 |
| **STC32** | MCS-251 | 未确认 | 未确认 | 未确认 | 未知 |

### 3.2 系列特征详解

#### STC89系列 (最老)

```yaml
握手响应长度: 46-50字节
编程数据包:   128字节
编程确认:     1-4字节
校验响应:     较短
特点:
  - 最简洁的协议
  - 无校验位
  - 无复杂功能
  - 超时时间较短
```

#### STC12系列

```yaml
握手响应长度: 50-58字节
编程数据包:   128字节
编程确认:     4-8字节
校验响应:     变长
特点:
  - 需要偶校验
  - 增加EEPROM支持
  - 包长度略有增加
  - 支持IAP功能
```

#### STC15系列

```yaml
握手响应长度: 52-64字节
编程数据包:   128字节
编程确认:     8-16字节
校验响应:     包含更多状态信息
特点:
  - 需要偶校验
  - 支持RC振荡器校准
  - 数据包更复杂
  - 状态信息更详细
```

#### STC8系列 (现代)

```yaml
握手响应长度: 64-74字节
编程数据包:   256字节 (部分型号)
编程确认:     16字节以上
校验响应:     包含详细状态
特点:
  - 功能最丰富
  - 支持更大编程包
  - 包长度最长
  - 协议最复杂
```

#### STC32系列 (MCS-251架构)

```yaml
握手响应长度: 待确认 (可能更长)
编程数据包:   待确认
编程确认:     待确认
特点:
  - 不同CPU架构
  - 协议可能有重大差异
  - 需要专门适配
```

---

## 4. 各阶段响应详解

### 4.1 握手阶段 (Handshake)

#### 响应数据组成

```c
// 典型握手响应结构 (以STC15为例)
struct handshake_response {
    uint8_t  header[2];         // 帧头: 0x46, 0xB9 (示例)
    uint8_t  length;            // 数据长度
    uint16_t magic;             // MCU型号识别码 (可变)
    uint8_t  version[4];        // Bootloader版本 (可变)
    uint32_t frequency;         // 工作频率 (可变)
    uint8_t  uid[8];            // 唯一ID (每片不同)
    uint8_t  options[N];        // 配置选项
    uint8_t  checksum[2];       // 校验和
};
```

#### 长度变化因素

- MCU系列不同
- Bootloader版本不同
- 功能特性差异 (是否支持校准、IAP等)

### 4.2 擦除阶段 (Erase)

#### 响应特征

```c
// 擦除响应通常较简短
struct erase_response {
    uint8_t status;      // 状态码
    uint8_t reserved[];  // 保留字节 (长度因系列而异)
    uint8_t checksum;    // 校验和
};
```

#### 各系列响应长度

- **STC89**: 1字节
- **STC12**: 4字节
- **STC15**: 8字节
- **STC8**: 16字节

### 4.3 编程阶段 (Program)

#### 响应特征

```c
// 编程阶段每包响应
struct program_response {
    uint8_t status;       // 状态 (成功/失败)
    uint8_t progress[];   // 进度信息 (可选，较新系列有)
    uint8_t checksum;     // 校验和
};
```

#### 编程包大小

- **STC89/12/15**: 128字节/包
- **STC8**: 128-256字节/包 (型号相关)
- **响应**: 回显确认或简短状态

### 4.4 校验阶段 (Verify)

#### 响应特征

```c
// 校验响应
struct verify_response {
    uint8_t  status;          // 校验结果
    uint32_t checksum_calc;   // 计算的校验和 (可变)
    uint8_t  details[];       // 详细信息 (较新系列)
    uint8_t  packet_checksum; // 包校验和
};
```

#### 响应长度

- **STC89**: 4字节
- **STC12**: 8字节
- **STC15**: 12字节
- **STC8**: 16字节+

---

## 5. 编程实现建议

### 5.1 协议配置结构体

```c
/**
 * @brief STC ISP协议配置结构
 */
typedef struct {
    // 握手阶段
    uint16_t handshake_response_len;     /**< 握手响应长度 */
    uint16_t handshake_min_len;          /**< 最小有效长度 */
    
    // 编程阶段
    uint16_t program_packet_size;        /**< 编程数据包大小 */
    uint16_t program_response_len;       /**< 编程响应长度 */
    
    // 擦除阶段
    uint16_t erase_response_len;         /**< 擦除响应长度 */
    
    // 校验阶段
    uint16_t verify_response_len;        /**< 校验响应长度 */
    
    // 协议特性
    bool     has_parity;                 /**< 是否需要校验位 */
    bool     has_calibration;            /**< 是否支持校准 */
    bool     is_mcs251;                  /**< 是否MCS-251架构 */
    
    // 超时设置 (毫秒)
    uint32_t handshake_timeout_ms;       /**< 握手超时 */
    uint32_t program_timeout_ms;         /**< 编程超时 */
    uint32_t erase_timeout_ms;           /**< 擦除超时 */
    uint32_t verify_timeout_ms;          /**< 校验超时 */
} stc_protocol_config_t;
```

### 5.2 协议配置表

```c
/**
 * @brief 各系列协议配置表
 */
const stc_protocol_config_t g_protocol_configs[] = {
    // STC89/11/10系列
    {
        .handshake_response_len = 46,
        .handshake_min_len      = 40,
        .program_packet_size    = 128,
        .program_response_len   = 1,
        .erase_response_len     = 1,
        .verify_response_len    = 4,
        .has_parity             = false,
        .has_calibration        = false,
        .is_mcs251              = false,
        .handshake_timeout_ms   = 100,
        .program_timeout_ms     = 500,
        .erase_timeout_ms       = 2000,
        .verify_timeout_ms      = 1000,
    },
    
    // STC12系列
    {
        .handshake_response_len = 54,
        .handshake_min_len      = 50,
        .program_packet_size    = 128,
        .program_response_len   = 4,
        .erase_response_len     = 4,
        .verify_response_len    = 8,
        .has_parity             = true,
        .has_calibration        = false,
        .is_mcs251              = false,
        .handshake_timeout_ms   = 100,
        .program_timeout_ms     = 500,
        .erase_timeout_ms       = 3000,
        .verify_timeout_ms      = 1500,
    },
    
    // STC15系列
    {
        .handshake_response_len = 58,
        .handshake_min_len      = 52,
        .program_packet_size    = 128,
        .program_response_len   = 8,
        .erase_response_len     = 8,
        .verify_response_len    = 12,
        .has_parity             = true,
        .has_calibration        = true,
        .is_mcs251              = false,
        .handshake_timeout_ms   = 100,
        .program_timeout_ms     = 1000,
        .erase_timeout_ms       = 5000,
        .verify_timeout_ms      = 2000,
    },
    
    // STC8系列
    {
        .handshake_response_len = 68,
        .handshake_min_len      = 60,
        .program_packet_size    = 256,
        .program_response_len   = 16,
        .erase_response_len     = 16,
        .verify_response_len    = 16,
        .has_parity             = true,
        .has_calibration        = true,
        .is_mcs251              = false,
        .handshake_timeout_ms   = 100,
        .program_timeout_ms     = 1000,
        .erase_timeout_ms       = 8000,
        .verify_timeout_ms      = 3000,
    },
};
```

### 5.3 协议选择函数

```c
/**
 * @brief 根据Magic码选择协议配置
 * @param magic MCU型号识别码
 * @return 协议配置指针，失败返回NULL
 */
const stc_protocol_config_t* stc_select_protocol(uint16_t magic)
{
    // STC89/STC10/STC11系列: 0xD2xx, 0xD3xx, 0xE2xx
    if ((magic & 0xF000) == 0xD000 || (magic & 0xF000) == 0xE000) {
        return &g_protocol_configs[0];  // 使用STC89配置
    }
    
    // STC12系列: 0xD1xx
    if ((magic & 0xFF00) == 0xD100) {
        return &g_protocol_configs[1];  // 使用STC12配置
    }
    
    // STC15系列: 0xF2xx, 0xF3xx, 0xF4xx, 0xF5xx
    if ((magic & 0xF000) == 0xF000 && (magic & 0x0F00) <= 0x0500) {
        return &g_protocol_configs[2];  // 使用STC15配置
    }
    
    // STC8系列: 0xF6xx, 0xF7xx (非STC32)
    if (((magic & 0xFF00) == 0xF600) || 
        ((magic & 0xFF00) == 0xF700 && (magic & 0x00F0) < 0x0080)) {
        return &g_protocol_configs[3];  // 使用STC8配置
    }
    
    // STC32系列: 0xF8xx (MCS-251架构)
    if ((magic & 0xFF00) == 0xF800 && (magic & 0x00F0) >= 0x0070) {
        // TODO: STC32系列需要特殊处理
        return NULL;  // 暂不支持
    }
    
    return NULL;  // 未知系列
}
```

### 5.4 弹性接收函数

```c
/**
 * @brief 接收可变长度的响应包
 * @param buffer 接收缓冲区
 * @param expected_len 期望长度
 * @param min_len 最小有效长度
 * @param max_len 最大允许长度
 * @param timeout_ms 总超时时间(毫秒)
 * @return 实际接收的字节数，失败返回0
 */
uint16_t stc_receive_response(
    uint8_t  *buffer, 
    uint16_t  expected_len,
    uint16_t  min_len,
    uint16_t  max_len,
    uint32_t  timeout_ms)
{
    uint16_t received = 0;
    uint32_t start_tick = hal_get_tick();
    uint32_t last_byte_tick = start_tick;
    const uint32_t inter_byte_timeout = 10;  // 字节间超时10ms
    
    while (received < max_len) {
        // 检查是否有数据
        if (hal_uart_data_available()) {
            buffer[received++] = hal_uart_read_byte();
            last_byte_tick = hal_get_tick();
            
            // 如果已接收到期望长度，再等待看是否还有数据
            if (received >= expected_len) {
                uint32_t wait_start = hal_get_tick();
                while ((hal_get_tick() - wait_start) < inter_byte_timeout) {
                    if (hal_uart_data_available()) {
                        break;  // 还有数据，继续接收
                    }
                }
                if (!hal_uart_data_available()) {
                    break;  // 确认没有更多数据
                }
            }
        }
        
        // 字节间超时检查
        if (received > 0) {
            if ((hal_get_tick() - last_byte_tick) > inter_byte_timeout) {
                break;  // 一段时间没有新数据，认为接收完成
            }
        }
        
        // 总超时检查
        if ((hal_get_tick() - start_tick) > timeout_ms) {
            break;  // 总体超时
        }
    }
    
    // 验证接收长度是否有效
    if (received < min_len) {
        return 0;  // 数据太少，无效
    }
    
    return received;
}
```

### 5.5 使用示例

```c
/**
 * @brief STC ISP握手示例
 */
bool stc_handshake_example(void)
{
    uint8_t response[128];
    uint16_t received_len;
    
    // 1. 发送握手命令
    stc_send_handshake_command();
    
    // 2. 接收响应 - 先用默认配置
    received_len = stc_receive_response(
        response,
        64,    // 期望长度(估计值)
        40,    // 最小长度
        128,   // 最大长度
        1000   // 1秒超时
    );
    
    if (received_len == 0) {
        return false;  // 握手失败
    }
    
    // 3. 解析Magic码
    uint16_t magic = (response[4] << 8) | response[5];  // 假设位置
    
    // 4. 选择协议配置
    const stc_protocol_config_t *config = stc_select_protocol(magic);
    if (config == NULL) {
        return false;  // 不支持的型号
    }
    
    // 5. 验证接收长度
    if (received_len < config->handshake_min_len) {
        return false;  // 响应长度不足
    }
    
    // 6. 后续使用该配置进行编程
    g_current_protocol = config;
    
    return true;
}
```

---

## 6. 协议配置参考

### 6.1 Magic码分布

| Magic范围 | 系列 | 架构 | 校验 | 校准 |
|-----------|------|------|------|------|
| 0xD1xx | STC12 | 8051 | 偶校验 | 否 |
| 0xD2xx | STC89/10 | 8051 | 无 | 否 |
| 0xD3xx | STC11 | 8051 | 无 | 否 |
| 0xE2xx | STC11 | 8051 | 无 | 否 |
| 0xF2xx-F5xx | STC15 | 8051 | 偶校验 | 是 |
| 0xF6xx | STC8/A | 8051+ | 偶校验 | 是 |
| 0xF7xx | STC8/H/G/C | 8051+ | 偶校验 | 是 |
| 0xF8xx (>=0x70) | STC32 | MCS-251 | 偶校验 | 是 |

### 6.2 UART配置

| 系列 | 数据位 | 校验位 | 停止位 | 推荐波特率 |
|------|--------|--------|--------|------------|
| STC89 | 8 | 无 | 1 | 9600-115200 |
| STC12 | 8 | 偶 | 1 | 9600-115200 |
| STC15 | 8 | 偶 | 1 | 9600-115200 |
| STC8 | 8 | 偶 | 1 | 9600-230400 |
| STC32 | 8 | 偶 | 1 | 待确认 |

### 6.3 超时时间建议

| 操作 | STC89 | STC12 | STC15 | STC8 |
|------|-------|-------|-------|------|
| 握手 | 100ms | 100ms | 100ms | 100ms |
| 擦除 | 2s | 3s | 5s | 8s |
| 编程(每包) | 500ms | 500ms | 1s | 1s |
| 校验 | 1s | 1.5s | 2s | 3s |

---

## 7. 注意事项

### 7.1 不要硬编码长度

❌ **错误做法**:
```c
#define HANDSHAKE_RESPONSE_LEN 50  // 只适用于某些型号
uint8_t buffer[HANDSHAKE_RESPONSE_LEN];
```

✅ **正确做法**:
```c
const stc_protocol_config_t *config = stc_select_protocol(magic);
uint8_t buffer[config->handshake_response_len];
```

### 7.2 使用协议检测

```c
// 步骤1: 握手时获取Magic码
uint16_t magic = stc_detect_mcu_magic();

// 步骤2: 根据Magic选择协议
const stc_protocol_config_t *config = stc_select_protocol(magic);

// 步骤3: 使用该配置进行后续操作
stc_program_with_config(config, firmware_data, firmware_size);
```

### 7.3 保持向后兼容

```c
// 对于未知型号，使用保守策略
if (config == NULL) {
    // 方案1: 使用最大可能长度
    config = &g_protocol_configs[3];  // 使用STC8配置作为兜底
    
    // 方案2: 提示用户手动选择
    config = user_select_protocol();
    
    // 方案3: 拒绝操作
    return ERROR_UNSUPPORTED_MCU;
}
```

### 7.4 校验和验证

```c
/**
 * @brief 验证响应包的校验和
 */
bool stc_verify_checksum(const uint8_t *data, uint16_t len)
{
    if (len < 2) {
        return false;
    }
    
    // 计算校验和 (具体算法根据系列不同)
    uint16_t calc_sum = 0;
    for (uint16_t i = 0; i < len - 2; i++) {
        calc_sum += data[i];
    }
    
    // 提取接收到的校验和
    uint16_t recv_sum = (data[len-2] << 8) | data[len-1];
    
    return (calc_sum == recv_sum);
}
```

### 7.5 日志记录

```c
// 记录实际响应数据，用于调试和分析
void log_response(const char *stage, const uint8_t *data, uint16_t len)
{
    printf("[%s] Received %d bytes: ", stage, len);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}
```

### 7.6 Bootloader版本差异

⚠️ **同一型号的不同Bootloader版本可能有差异**：

```c
// 示例：STC15F104W
// Bootloader 7.1Q:  响应包 52字节
// Bootloader 7.2.5Q: 响应包 61字节 (+9字节)

// 处理策略：使用min_len和max_len区间
```

---

## 8. 参考资料

### 8.1 开源项目

- **stcgal**: https://github.com/grigorig/stcgal  
  Python实现的STC ISP编程工具，支持多个系列

- **stc8prog**: https://github.com/stawel/stc8prog  
  专门针对STC8系列的编程工具

### 8.2 相关文档

- STC官方ISP软件
- STC MCU数据手册
- 反向工程文档和社区资料

### 8.3 本项目文件

- `stc_mcu_database.h/c`: MCU型号数据库
- `stc_isp_protocol.h/c`: ISP协议实现
- `stc_hal.h/c`: 硬件抽象层

---

## 9. 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| 1.0 | 2025-11-30 | STC ISP项目组 | 初始版本 |

---

## 10. 附录

### 10.1 常见问题

**Q: 为什么不能硬编码响应长度？**  
A: 不同系列和版本的响应长度差异很大，硬编码会导致兼容性问题。

**Q: 如何处理未知型号？**  
A: 建议使用最大长度接收，或提示用户手动选择协议。

**Q: 校验和算法在所有系列都一样吗？**  
A: 不一定，建议根据系列选择对应的校验算法。

**Q: STC32系列如何处理？**  
A: STC32采用不同架构，建议等待更多技术资料或使用专门工具。

### 10.2 术语表

- **ISP**: In-System Programming，在系统编程
- **IAP**: In-Application Programming，在应用编程
- **Magic码**: MCU型号识别码，用于区分不同型号
- **Bootloader**: 引导加载程序，MCU内置的ISP程序
- **校验和**: 用于验证数据完整性的校验值

---

**文档结束**
