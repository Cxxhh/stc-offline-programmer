# STC8 烧录算法实现要点（移植参考）

## 1. 数据包格式

### 发送数据包结构
```
[0x46 0xB9] [0x6A] [长度H 长度L] [数据...] [校验H 校验L] [0x16]
```

### 接收数据包结构
```
[0x46 0xB9] [0x68] [长度H 长度L] [数据...] [校验H 校验L] [0x16]
```

### 校验和计算
```c
uint16_t checksum = 0;
for (int i = 2; i < packet_len - 3; i++) {
    checksum += packet[i];
}
checksum &= 0xFFFF;
```

## 2. 关键命令码

| 命令 | 功能 | 请求格式 | 响应格式 |
|------|------|----------|----------|
| 0x50 | 状态包 | (MCU主动发送) | 0x50 + 状态数据 |
| 0x00 | 频率校准 | 0x00 + 长度 + 测试点 | 0x00 + 计数值 |
| 0x01 | 切换波特率 | 0x01 0x00 0x00 + BRT + trim + iap | 0x01 |
| 0x03 | 擦除Flash | 0x03 + [0x00/0x01] + [0x5A 0xA5] | 0x03 + UID |
| 0x22 | 首块编程 | 0x22 + 地址 + [0x5A 0xA5] + 数据 | 0x02 0x54 |
| 0x02 | 后续块编程 | 0x02 + 地址 + [0x5A 0xA5] + 数据 | 0x02 0x54 |
| 0x07 | 完成编程 | 0x07 0x00 0x00 0x5A 0xA5 | 0x07 0x54 |
| 0x04 | 配置选项 | 0x04 0x00 0x00 + [0x5A 0xA5] + 选项 | 0x04 0x54 |
| 0xFF | 断开连接 | 0xFF | (无响应) |

## 3. 状态包解析

```c
typedef struct {
    uint32_t mcu_clock_hz;      // 字节 1-4: MCU时钟频率
    uint8_t  bsl_version;       // 字节 17: BSL主版本
    uint8_t  bsl_stepping;      // 字节 18: BSL步进版本
    uint8_t  bsl_minor;         // 字节 22低4位: BSL次版本
    uint16_t wakeup_freq;       // 字节 23-24: 唤醒频率
    uint16_t reference_voltage; // 字节 35-36: 参考电压(mV)
    uint8_t  mfg_year;          // 字节 37: 制造年份(BCD)
    uint8_t  mfg_month;          // 字节 38: 制造月份(BCD)
    uint8_t  mfg_day;           // 字节 39: 制造日期(BCD)
    uint8_t  mcs[5];            // 字节 9-12, 15-17: MCS选项字节
} stc8_status_t;
```

## 4. 频率校准算法

### 第一轮校准
```c
// 发送校准挑战
uint8_t packet[] = {
    0x00,                    // 命令
    0x0C,                    // 长度(12个测试点)
    0x00, 0x00,              // 测试点 0
    0x17, 0x00,              // 测试点 1 (23)
    0x2E, 0x00,              // 测试点 2 (46)
    0x45, 0x00,              // 测试点 3 (69)
    0x5C, 0x00,              // 测试点 4 (92)
    0x73, 0x00,              // 测试点 5 (115)
    0x8A, 0x00,              // 测试点 6 (138)
    0xA1, 0x00,              // 测试点 7 (161)
    0xB8, 0x00,              // 测试点 8 (184)
    0xCF, 0x00,              // 测试点 9 (207)
    0xE6, 0x00,              // 测试点 10 (230)
    0xFF, 0x00               // 测试点 11 (255)
};

// 接收响应，解析计数值
// 响应格式: 0x00 + 长度 + [计数值1H 计数值1L] ... [计数值12H 计数值12L]

// 选择合适的分频器和范围
uint8_t divider = 1;
uint8_t trim_base = 0;
for (divider = 1; divider <= 5; divider++) {
    uint32_t target_count = (target_freq * divider) / (baud_handshake / 2);
    // 在响应中查找包含 target_count 的范围
    if (find_range(response, target_count, &trim_base)) {
        break;
    }
}
```

### 第二轮校准
```c
// 发送精细校准挑战
uint8_t packet[] = {
    0x00,                    // 命令
    0x0C,                    // 长度(12个测试点)
    // 每个 trim 值测试 4 个 fine adjustment
    trim_base-1, 0x00,       // trim-1, fine=0
    trim_base-1, 0x01,       // trim-1, fine=1
    trim_base-1, 0x02,       // trim-1, fine=2
    trim_base-1, 0x03,       // trim-1, fine=3
    trim_base,   0x00,       // trim, fine=0
    trim_base,   0x01,       // trim, fine=1
    trim_base,   0x02,       // trim, fine=2
    trim_base,   0x03,       // trim, fine=3
    trim_base+1, 0x00,       // trim+1, fine=0
    trim_base+1, 0x01,       // trim+1, fine=1
    trim_base+1, 0x02,       // trim+1, fine=2
    trim_base+1, 0x03        // trim+1, fine=3
};

// 选择最接近目标频率的 trim 值
uint8_t best_trim = 0;
uint8_t best_fine = 0;
uint32_t best_count = 0xFFFFFFFF;
// ... 比较所有测试点，选择最佳值
```

### 切换波特率
```c
// 计算 BRT (波特率寄存器值)
uint16_t brt = 65536 - (24000000 / (target_baud * 4));

// 发送切换命令
uint8_t packet[] = {
    0x01,                    // 命令
    0x00, 0x00,              // 保留
    (brt >> 8) & 0xFF,       // BRT高字节
    brt & 0xFF,              // BRT低字节
    best_fine,               // trim fine值
    best_trim,               // trim base值
    0x98                     // IAP等待时间(固定值)
};

// 切换串口波特率
uart_set_baudrate(target_baud);
```

## 5. Flash 擦除

```c
uint8_t erase_packet[] = {
    0x03,                    // 命令
    0x00,                    // 0x00=仅Flash, 0x01=Flash+EEPROM
    0x00, 0x5A, 0xA5         // BSL 7.2+需要
};

// 响应: 0x03 + UID(7字节)
```

## 6. Flash 编程

```c
#define BLOCK_SIZE 64

for (uint32_t addr = 0; addr < data_len; addr += BLOCK_SIZE) {
    uint8_t packet[3 + 2 + 2 + BLOCK_SIZE];
    uint16_t offset = 0;
    
    // 命令字节
    if (addr == 0) {
        packet[offset++] = 0x22;  // 首块
    } else {
        packet[offset++] = 0x02;  // 后续块
    }
    
    // 地址(大端)
    packet[offset++] = (addr >> 8) & 0xFF;
    packet[offset++] = addr & 0xFF;
    
    // BSL 7.2+需要
    if (bsl_version >= 0x72) {
        packet[offset++] = 0x5A;
        packet[offset++] = 0xA5;
    }
    
    // 数据(64字节，不足补0)
    uint16_t copy_len = (data_len - addr > BLOCK_SIZE) ? BLOCK_SIZE : (data_len - addr);
    memcpy(&packet[offset], &data[addr], copy_len);
    offset += copy_len;
    memset(&packet[offset], 0, BLOCK_SIZE - copy_len);
    offset += (BLOCK_SIZE - copy_len);
    
    // 发送数据包
    send_packet(packet, offset);
    
    // 等待响应: 0x02 0x54
    uint8_t response[2];
    receive_packet(response, 2);
    if (response[0] != 0x02 || response[1] != 0x54) {
        // 错误处理
    }
}

// BSL 7.2+需要发送完成包
if (bsl_version >= 0x72) {
    uint8_t finish_packet[] = {0x07, 0x00, 0x00, 0x5A, 0xA5};
    send_packet(finish_packet, 5);
    // 等待响应: 0x07 0x54
}
```

## 7. 选项配置

```c
typedef struct {
    uint32_t trim_frequency;  // 校准后的频率
    uint8_t  trim_value[2];   // trim值 [fine, base]
    uint8_t  trim_divider;    // 分频器
    uint8_t  mcs[5];          // MCS选项字节
} stc8_options_t;

void build_options_packet(uint8_t *packet, stc8_options_t *opts) {
    memset(packet, 0xFF, 40);
    
    packet[3] = 0x00;
    packet[6] = 0x00;
    packet[22] = 0x00;
    
    // 频率(大端32位)
    packet[24] = (opts->trim_frequency >> 24) & 0xFF;
    packet[25] = (opts->trim_frequency >> 16) & 0xFF;
    packet[26] = (opts->trim_frequency >> 8) & 0xFF;
    packet[27] = opts->trim_frequency & 0xFF;
    
    // trim值
    packet[28] = opts->trim_value[0];  // fine
    packet[29] = opts->trim_value[1];  // base
    
    // 分频器
    packet[30] = opts->trim_divider;
    
    // MCS字节
    packet[32] = opts->mcs[0];
    packet[36] = opts->mcs[1];
    packet[37] = opts->mcs[2];
    packet[38] = opts->mcs[3];
    packet[39] = opts->mcs[4];
}

// 发送选项配置
uint8_t packet[3 + 2 + 40];
packet[0] = 0x04;
packet[1] = 0x00;
packet[2] = 0x00;
if (bsl_version >= 0x72) {
    packet[3] = 0x5A;
    packet[4] = 0xA5;
    build_options_packet(&packet[5], &options);
    send_packet(packet, 5 + 40);
} else {
    build_options_packet(&packet[3], &options);
    send_packet(packet, 3 + 40);
}

// 等待响应: 0x04 0x54
```

## 8. MCS 选项字节位定义

### MCS0 (字节32)
- bit 0: ~BSL使能 (0=禁用, 1=使能)
- bit 1: EEPROM擦除使能

### MCS1 (字节36)
- bit 1: 时钟增益 (0=低, 1=高)
- bit 2: EPWM推挽使能
- bit 3: P2.0启动后状态
- bit 4: TXD信号源选择
- bit 5: P3.7推挽使能
- bit 6: UART1重映射

### MCS2 (字节37)
- bit 0-1: 低电压阈值 (0=2.20V, 1=2.40V, 2=2.70V, 3=3.00V)
- bit 4: ~复位引脚使能
- bit 6: ~低电压复位使能

### MCS3 (字节38)
- bit 0-2: 看门狗预分频 (0=2, 1=4, 2=8, ..., 7=256)
- bit 3: ~看门狗在空闲时停止
- bit 5: ~看门狗上电复位使能

### MCS4 (字节39)
- EEPROM/代码空间分割点 (页数)

## 9. 完整流程伪代码

```c
void stc8_program(uint8_t *firmware, uint32_t len) {
    // 1. 初始化串口
    uart_init(2400, PARITY_EVEN);
    
    // 2. 同步并读取状态
    uart_send_pulse(0x7F);
    stc8_status_t status;
    read_status_packet(&status);
    
    // 3. 频率校准
    stc8_trim_t trim;
    calibrate_frequency(&trim, target_freq);
    
    // 4. 切换波特率
    switch_baudrate(target_baud, &trim);
    
    // 5. 擦除Flash
    erase_flash(FLASH_ONLY);
    
    // 6. 编程Flash
    program_flash(firmware, len);
    
    // 7. 配置选项
    stc8_options_t opts;
    build_options(&opts, &trim, &status);
    program_options(&opts);
    
    // 8. 断开连接
    disconnect();
}
```

## 10. 注意事项

1. **字节序**：所有多字节数据使用大端序 (Big-Endian)
2. **校验和**：16位累加校验，包含长度字段但不包含帧头帧尾
3. **超时**：每个操作需要设置适当的超时时间
4. **BSL版本**：BSL 7.2+ 需要额外的 0x5A 0xA5 字节
5. **固定频率**：编程时固定使用 24MHz，不受用户频率影响
6. **块大小**：Flash编程块大小为64字节，需要对齐
7. **分频器**：低频率通过分频器实现，校准在20-30MHz范围


