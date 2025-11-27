# STC各型号烧录算法整合文档

## 目录
1. [算法概述](#算法概述)
2. [STC89系列烧录算法](#stc89系列烧录算法)
3. [STC89A系列烧录算法](#stc89a系列烧录算法)
4. [STC12系列烧录算法](#stc12系列烧录算法)
5. [STC15系列烧录算法](#stc15系列烧录算法)
6. [STC8系列烧录算法](#stc8系列烧录算法)
7. [STC32系列烧录算法](#stc32系列烧录算法)
8. [USB协议烧录算法](#usb协议烧录算法)
9. [算法对比总结](#算法对比总结)

---

## 算法概述

STC烧录算法基于ISP（In-System Programming）协议，通过UART或USB接口与MCU的引导加载程序（BSL）通信。不同型号的MCU使用不同的协议变体，但基本流程相似。

### 通用烧录流程

```
1. 连接与同步
2. 读取状态包
3. 识别MCU型号
4. 初始化选项
5. 握手与波特率切换
6. 擦除Flash
7. 编程Flash
8. 设置选项字节
9. 断开连接
```

---

## STC89系列烧录算法

### 协议特点
- **校验方式**：单字节校验和
- **校验位**：无校验（PARITY_NONE）
- **块大小**：128字节
- **BRT寄存器**：16位（65536 - 计算值）

### 详细算法步骤

#### 1. 数据包构建算法

```c
// 写入数据包构建
void build_write_packet(uint8_t* packet_data, uint16_t data_len, uint8_t* output) {
    uint16_t pos = 0;
    
    // 帧起始
    output[pos++] = 0x46;
    output[pos++] = 0xB9;
    
    // 方向标志（Host -> MCU）
    output[pos++] = 0x6A;
    
    // 数据长度（大端序）
    uint16_t total_len = data_len + 5;
    output[pos++] = (total_len >> 8) & 0xFF;
    output[pos++] = total_len & 0xFF;
    
    // 数据载荷
    memcpy(&output[pos], packet_data, data_len);
    pos += data_len;
    
    // 校验和（单字节）
    uint8_t checksum = 0;
    for (int i = 2; i < pos; i++) {
        checksum += output[i];
    }
    output[pos++] = checksum & 0xFF;
    
    // 帧结束
    output[pos++] = 0x16;
}
```

#### 2. 数据包解析算法

```c
// 读取数据包解析
int parse_read_packet(uint8_t* input, uint16_t input_len, uint8_t* payload) {
    // 检查帧起始
    if (input[0] != 0x46 || input[1] != 0xB9) {
        return -1; // 帧起始错误
    }
    
    // 检查方向标志
    if (input[2] != 0x68) {
        return -2; // 方向错误
    }
    
    // 读取长度
    uint16_t packet_len = (input[3] << 8) | input[4];
    
    // 验证校验和
    uint8_t calc_checksum = 0;
    for (int i = 2; i < packet_len - 2; i++) {
        calc_checksum += input[i];
    }
    
    if (calc_checksum != input[packet_len - 2]) {
        return -3; // 校验和错误
    }
    
    // 提取载荷（去掉最后一个字节，它是校验和的一部分）
    uint16_t payload_len = packet_len - 6;
    memcpy(payload, &input[5], payload_len - 1);
    
    return payload_len - 1;
}
```

#### 3. 频率计算算法

```c
// 计算MCU时钟频率
float calculate_mcu_frequency(uint8_t* status_packet, uint16_t baud_handshake) {
    // 检查CPU模式（6T或12T）
    uint8_t cpu_6t = !(status_packet[19] & 1);
    float cpu_t = cpu_6t ? 6.0 : 12.0;
    
    // 计算8次频率计数的平均值
    uint32_t freq_counter_sum = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t count = (status_packet[1 + 2*i] << 8) | status_packet[2 + 2*i];
        freq_counter_sum += count;
    }
    float freq_counter = freq_counter_sum / 8.0;
    
    // 计算实际频率
    float mcu_clock_hz = (baud_handshake * freq_counter * cpu_t) / 7.0;
    
    return mcu_clock_hz;
}
```

#### 4. 波特率计算算法

```c
// 计算BRT寄存器值
typedef struct {
    uint16_t brt;
    uint8_t brt_csum;
    uint8_t iap_wait;
    uint8_t delay;
} baud_params_t;

baud_params_t calculate_baud_stc89(float mcu_clock_hz, uint16_t baud_transfer, uint8_t cpu_6t) {
    baud_params_t params;
    
    // 采样率：6T模式为16，12T模式为32
    uint8_t sample_rate = cpu_6t ? 16 : 32;
    
    // 计算BRT寄存器值
    params.brt = 65536 - (uint16_t)round(mcu_clock_hz / (baud_transfer * sample_rate));
    
    // 计算BRT校验和
    params.brt_csum = (2 * (256 - (params.brt >> 8))) & 0xFF;
    
    // 计算IAP等待状态
    params.iap_wait = 0x80;
    if (mcu_clock_hz < 5E6) params.iap_wait = 0x83;
    else if (mcu_clock_hz < 10E6) params.iap_wait = 0x82;
    else if (mcu_clock_hz < 20E6) params.iap_wait = 0x81;
    
    // MCU延时
    params.delay = 0xA0;
    
    return params;
}
```

#### 5. 握手算法

```c
// STC89握手流程
int handshake_stc89(serial_port_t* port, uint16_t baud_transfer, 
                    float mcu_clock_hz, uint8_t cpu_6t, uint16_t mcu_magic) {
    baud_params_t params = calculate_baud_stc89(mcu_clock_hz, baud_transfer, cpu_6t);
    
    // 步骤1：测试新波特率
    uint8_t test_packet[20];
    test_packet[0] = 0x8F;
    test_packet[1] = (params.brt >> 8) & 0xFF;
    test_packet[2] = params.brt & 0xFF;
    test_packet[3] = 0xFF - (params.brt >> 8);
    test_packet[4] = params.brt_csum;
    test_packet[5] = params.delay;
    test_packet[6] = params.iap_wait;
    
    uint8_t packet[256];
    build_write_packet(test_packet, 7, packet);
    serial_write(port, packet, get_packet_length(packet));
    
    delay_ms(100);
    serial_set_baudrate(port, baud_transfer);
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0 || parse_read_packet(response, len, test_packet) < 0) {
        return -1;
    }
    if (test_packet[0] != 0x8F) {
        return -2;
    }
    
    serial_set_baudrate(port, 2400); // 切回握手波特率
    
    // 步骤2：切换到新波特率
    uint8_t switch_packet[20];
    switch_packet[0] = 0x8E;
    switch_packet[1] = (params.brt >> 8) & 0xFF;
    switch_packet[2] = params.brt & 0xFF;
    switch_packet[3] = 0xFF - (params.brt >> 8);
    switch_packet[4] = params.brt_csum;
    switch_packet[5] = params.delay;
    
    build_write_packet(switch_packet, 6, packet);
    serial_write(port, packet, get_packet_length(packet));
    
    delay_ms(100);
    serial_set_baudrate(port, baud_transfer);
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0 || parse_read_packet(response, len, switch_packet) < 0) {
        return -3;
    }
    if (switch_packet[0] != 0x8E) {
        return -4;
    }
    
    // 步骤3：Ping-Pong测试（4次）
    uint8_t ping_packet[20];
    ping_packet[0] = 0x80;
    ping_packet[1] = 0x00;
    ping_packet[2] = 0x00;
    ping_packet[3] = 0x36;
    ping_packet[4] = 0x01;
    ping_packet[5] = (mcu_magic >> 8) & 0xFF;
    ping_packet[6] = mcu_magic & 0xFF;
    
    for (int i = 0; i < 4; i++) {
        build_write_packet(ping_packet, 7, packet);
        serial_write(port, packet, get_packet_length(packet));
        
        len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0 || parse_read_packet(response, len, ping_packet) < 0) {
            return -5;
        }
        if (ping_packet[0] != 0x80) {
            return -6;
        }
    }
    
    return 0; // 成功
}
```

#### 6. 擦除Flash算法

```c
// STC89擦除Flash
int erase_flash_stc89(serial_port_t* port, uint32_t erase_size) {
    // 计算块数（每512字节为1块，需要擦除2倍块数）
    uint16_t blks = ((erase_size + 511) / 512) * 2;
    
    uint8_t erase_packet[20];
    erase_packet[0] = 0x84;
    erase_packet[1] = blks & 0xFF;
    erase_packet[2] = 0x33;
    erase_packet[3] = 0x33;
    erase_packet[4] = 0x33;
    erase_packet[5] = 0x33;
    erase_packet[6] = 0x33;
    erase_packet[7] = 0x33;
    
    uint8_t packet[256];
    build_write_packet(erase_packet, 8, packet);
    serial_write(port, packet, get_packet_length(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 15000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x80) {
        return -2;
    }
    
    return 0; // 成功
}
```

#### 7. 编程Flash算法

```c
// STC89编程Flash
int program_flash_stc89(serial_port_t* port, uint8_t* data, uint32_t data_len) {
    const uint16_t BLOCK_SIZE = 128;
    
    for (uint32_t i = 0; i < data_len; i += BLOCK_SIZE) {
        uint16_t block_len = (data_len - i < BLOCK_SIZE) ? (data_len - i) : BLOCK_SIZE;
        
        // 构建写入数据包
        uint8_t write_packet[256];
        uint16_t pos = 0;
        
        // 3字节填充
        write_packet[pos++] = 0x00;
        write_packet[pos++] = 0x00;
        write_packet[pos++] = 0x00;
        
        // 地址（大端序）
        write_packet[pos++] = (i >> 8) & 0xFF;
        write_packet[pos++] = i & 0xFF;
        
        // 块大小（大端序）
        write_packet[pos++] = (BLOCK_SIZE >> 8) & 0xFF;
        write_packet[pos++] = BLOCK_SIZE & 0xFF;
        
        // 数据
        memcpy(&write_packet[pos], &data[i], block_len);
        pos += block_len;
        
        // 填充到固定长度
        while (pos < BLOCK_SIZE + 7) {
            write_packet[pos++] = 0x00;
        }
        
        // 计算数据校验和
        uint8_t data_csum = 0;
        for (int j = 7; j < pos; j++) {
            data_csum += write_packet[j];
        }
        
        // 构建完整数据包
        uint8_t packet[512];
        build_write_packet(write_packet, pos, packet);
        
        // 发送数据包
        serial_write(port, packet, get_packet_length(packet));
        
        // 读取响应
        uint8_t response[256];
        int len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0) return -1;
        
        uint8_t payload[256];
        int payload_len = parse_read_packet(response, len, payload);
        if (payload_len < 1 || payload[0] != 0x80) {
            return -2;
        }
        if (payload_len < 2 || payload[1] != data_csum) {
            return -3; // 校验和不匹配
        }
        
        // 更新进度
        update_progress(i, BLOCK_SIZE, data_len);
    }
    
    return 0; // 成功
}
```

#### 8. 设置选项字节算法

```c
// STC89设置选项字节
int program_options_stc89(serial_port_t* port, uint8_t msr) {
    uint8_t option_packet[10];
    option_packet[0] = 0x8D;
    option_packet[1] = msr;
    option_packet[2] = 0xFF;
    option_packet[3] = 0xFF;
    option_packet[4] = 0xFF;
    
    uint8_t packet[256];
    build_write_packet(option_packet, 5, packet);
    serial_write(port, packet, get_packet_length(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x8D) {
        return -2;
    }
    
    return 0; // 成功
}
```

---

## STC89A系列烧录算法

### 协议特点
- **校验方式**：双字节校验和
- **校验位**：初始无校验，握手后切换为偶校验
- **块大小**：128字节
- **BRT寄存器**：16位（65536 - 计算值）

### 关键差异算法

#### 1. 数据包构建（双字节校验和）

```c
// STC89A写入数据包构建
void build_write_packet_stc89a(uint8_t* packet_data, uint16_t data_len, uint8_t* output) {
    uint16_t pos = 0;
    
    // 帧起始
    output[pos++] = 0x46;
    output[pos++] = 0xB9;
    
    // 方向标志
    output[pos++] = 0x6A;
    
    // 数据长度（大端序）
    uint16_t total_len = data_len + 6;
    output[pos++] = (total_len >> 8) & 0xFF;
    output[pos++] = total_len & 0xFF;
    
    // 数据载荷
    memcpy(&output[pos], packet_data, data_len);
    pos += data_len;
    
    // 校验和（双字节，大端序）
    uint16_t checksum = 0;
    for (int i = 2; i < pos; i++) {
        checksum += output[i];
    }
    output[pos++] = (checksum >> 8) & 0xFF;
    output[pos++] = checksum & 0xFF;
    
    // 帧结束
    output[pos++] = 0x16;
}
```

#### 2. 握手算法（简化版）

```c
// STC89A握手流程
int handshake_stc89a(serial_port_t* port, uint16_t baud_transfer, 
                     float mcu_clock_hz) {
    // 计算BRT
    uint8_t sample_rate = 32;
    uint16_t brt = 65536 - (uint16_t)round(mcu_clock_hz / (baud_transfer * sample_rate));
    
    // 计算IAP等待状态
    uint8_t iap_wait = 0x80;
    if (mcu_clock_hz < 10E6) iap_wait = 0x83;
    else if (mcu_clock_hz < 30E6) iap_wait = 0x82;
    else if (mcu_clock_hz < 50E6) iap_wait = 0x81;
    
    // 步骤1：测试新波特率
    uint8_t test_packet[10];
    test_packet[0] = 0x01;
    test_packet[1] = (brt >> 8) & 0xFF;
    test_packet[2] = brt & 0xFF;
    test_packet[3] = iap_wait;
    
    uint8_t packet[256];
    build_write_packet_stc89a(test_packet, 4, packet);
    serial_write(port, packet, get_packet_length_stc89a(packet));
    
    delay_ms(200);
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet_stc89a(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x01) {
        return -2;
    }
    
    // 切换波特率
    serial_set_baudrate(port, baud_transfer);
    serial_set_parity(port, PARITY_EVEN); // 切换为偶校验
    
    // 步骤2：Ping-Pong测试
    uint8_t ping_packet[10];
    ping_packet[0] = 0x05;
    ping_packet[1] = 0x00;
    ping_packet[2] = 0x00;
    ping_packet[3] = 0x46;
    ping_packet[4] = 0xB9;
    
    build_write_packet_stc89a(ping_packet, 5, packet);
    serial_write(port, packet, get_packet_length_stc89a(packet));
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -3;
    
    payload_len = parse_read_packet_stc89a(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x05) {
        return -4;
    }
    
    return 0; // 成功
}
```

#### 3. 擦除Flash算法

```c
// STC89A擦除Flash（全擦除）
int erase_flash_stc89a(serial_port_t* port) {
    uint8_t erase_packet[10];
    erase_packet[0] = 0x03;
    erase_packet[1] = 0x00;
    erase_packet[2] = 0x00;
    erase_packet[3] = 0x46;
    erase_packet[4] = 0xB9;
    
    uint8_t packet[256];
    build_write_packet_stc89a(erase_packet, 5, packet);
    serial_write(port, packet, get_packet_length_stc89a(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 15000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet_stc89a(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x03) {
        return -2;
    }
    
    // 提取MCU ID（7字节）
    if (payload_len >= 8) {
        extract_mcu_id(&payload[1], 7);
    }
    
    return 0; // 成功
}
```

#### 4. 编程Flash算法

```c
// STC89A编程Flash
int program_flash_stc89a(serial_port_t* port, uint8_t* data, uint32_t data_len) {
    const uint16_t BLOCK_SIZE = 128;
    uint16_t page = 0;
    
    for (uint32_t i = 0; i < data_len; i += BLOCK_SIZE) {
        uint16_t block_len = (data_len - i < BLOCK_SIZE) ? (data_len - i) : BLOCK_SIZE;
        
        uint8_t write_packet[256];
        uint16_t pos = 0;
        
        if (page == 0) {
            // 首块
            write_packet[pos++] = 0x22;
            write_packet[pos++] = 0x00;
            write_packet[pos++] = 0x00;
        } else {
            // 后续块
            write_packet[pos++] = 0x02;
            uint16_t addr = 128 * page;
            write_packet[pos++] = (addr >> 8) & 0xFF;
            write_packet[pos++] = addr & 0xFF;
        }
        
        // 魔术字
        write_packet[pos++] = 0x46;
        write_packet[pos++] = 0xB9;
        
        // 数据
        memcpy(&write_packet[pos], &data[i], block_len);
        pos += block_len;
        
        // 构建完整数据包
        uint8_t packet[512];
        build_write_packet_stc89a(write_packet, pos, packet);
        
        // 发送数据包
        serial_write(port, packet, get_packet_length_stc89a(packet));
        
        // 读取响应
        uint8_t response[256];
        int len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0) return -1;
        
        uint8_t payload[256];
        int payload_len = parse_read_packet_stc89a(response, len, payload);
        if (payload_len < 1 || payload[0] != 0x02) {
            return -2;
        }
        
        page++;
        update_progress(i, BLOCK_SIZE, data_len);
    }
    
    return 0; // 成功
}
```

---

## STC12系列烧录算法

### 协议特点
- **校验方式**：双字节校验和
- **校验位**：偶校验（PARITY_EVEN）
- **块大小**：128字节
- **BRT寄存器**：8位（256 - 计算值）
- **擦除倒计时**：0x0D（从0x80倒计时）

### 关键算法

#### 1. 波特率计算算法

```c
// STC12波特率计算
baud_params_t calculate_baud_stc12(float mcu_clock_hz, uint16_t baud_transfer) {
    baud_params_t params;
    
    // BRT寄存器为8位
    params.brt = 256 - (uint8_t)round(mcu_clock_hz / (baud_transfer * 16));
    
    if (params.brt <= 1 || params.brt > 255) {
        // 错误：无法设置该波特率
        params.brt = 0;
        return params;
    }
    
    // BRT校验和
    params.brt_csum = (2 * (256 - params.brt)) & 0xFF;
    
    // IAP等待状态（根据频率）
    params.iap_wait = get_iap_delay(mcu_clock_hz);
    
    // MCU延时
    params.delay = 0x80;
    
    return params;
}

// IAP等待状态计算
uint8_t get_iap_delay(float clock_hz) {
    if (clock_hz < 1E6) return 0x87;
    else if (clock_hz < 2E6) return 0x86;
    else if (clock_hz < 3E6) return 0x85;
    else if (clock_hz < 6E6) return 0x84;
    else if (clock_hz < 12E6) return 0x83;
    else if (clock_hz < 20E6) return 0x82;
    else if (clock_hz < 24E6) return 0x81;
    else return 0x80;
}
```

#### 2. 握手算法

```c
// STC12握手流程
int handshake_stc12(serial_port_t* port, uint16_t baud_transfer, 
                    float mcu_clock_hz, uint16_t mcu_magic) {
    baud_params_t params = calculate_baud_stc12(mcu_clock_hz, baud_transfer);
    
    // 步骤1：发送握手请求
    uint8_t handshake_req[10];
    handshake_req[0] = 0x50;
    handshake_req[1] = 0x00;
    handshake_req[2] = 0x00;
    handshake_req[3] = 0x36;
    handshake_req[4] = 0x01;
    handshake_req[5] = (mcu_magic >> 8) & 0xFF;
    handshake_req[6] = mcu_magic & 0xFF;
    
    uint8_t packet[256];
    build_write_packet_stc12(handshake_req, 7, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet_stc12(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x8F) {
        return -2;
    }
    
    // 步骤2：测试新设置
    uint8_t test_packet[10];
    test_packet[0] = 0x8F;
    test_packet[1] = 0xC0;
    test_packet[2] = params.brt;
    test_packet[3] = 0x3F;
    test_packet[4] = params.brt_csum;
    test_packet[5] = params.delay;
    test_packet[6] = params.iap_wait;
    
    build_write_packet_stc12(test_packet, 7, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    serial_set_baudrate(port, baud_transfer);
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -3;
    
    payload_len = parse_read_packet_stc12(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x8F) {
        return -4;
    }
    
    serial_set_baudrate(port, 2400); // 切回
    
    // 步骤3：切换到新设置
    uint8_t switch_packet[10];
    switch_packet[0] = 0x8E;
    switch_packet[1] = 0xC0;
    switch_packet[2] = params.brt;
    switch_packet[3] = 0x3F;
    switch_packet[4] = params.brt_csum;
    switch_packet[5] = params.delay;
    
    build_write_packet_stc12(switch_packet, 6, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    serial_set_baudrate(port, baud_transfer);
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -5;
    
    payload_len = parse_read_packet_stc12(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x84) {
        return -6;
    }
    
    return 0; // 成功
}
```

#### 3. 擦除Flash算法

```c
// STC12擦除Flash
int erase_flash_stc12(serial_port_t* port, uint32_t erase_size, uint32_t flash_size) {
    uint16_t blks = ((erase_size + 511) / 512) * 2;
    uint16_t size = ((flash_size + 511) / 512) * 2;
    
    // 构建擦除数据包
    uint8_t erase_packet[256];
    uint16_t pos = 0;
    
    erase_packet[pos++] = 0x84;
    erase_packet[pos++] = 0xFF;
    erase_packet[pos++] = 0x00;
    erase_packet[pos++] = blks & 0xFF;
    erase_packet[pos++] = 0x00;
    erase_packet[pos++] = 0x00;
    erase_packet[pos++] = size & 0xFF;
    
    // 填充19字节的0x00
    for (int i = 0; i < 19; i++) {
        erase_packet[pos++] = 0x00;
    }
    
    // 倒计时序列（从0x80到0x0D）
    for (int i = 0x80; i >= 0x0D; i--) {
        erase_packet[pos++] = i;
    }
    
    uint8_t packet[512];
    build_write_packet_stc12(erase_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 15000);
    if (len < 0) return -1;
    
    uint8_t payload[256];
    int payload_len = parse_read_packet_stc12(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x00) {
        return -2;
    }
    
    // 提取UID（如果存在）
    if (payload_len >= 8) {
        extract_uid(&payload[1], 7);
    }
    
    return 0; // 成功
}
```

#### 4. 编程Flash算法

```c
// STC12编程Flash
int program_flash_stc12(serial_port_t* port, uint8_t* data, uint32_t data_len) {
    const uint16_t BLOCK_SIZE = 128;
    
    for (uint32_t i = 0; i < data_len; i += BLOCK_SIZE) {
        uint16_t block_len = (data_len - i < BLOCK_SIZE) ? (data_len - i) : BLOCK_SIZE;
        
        uint8_t write_packet[256];
        uint16_t pos = 0;
        
        // 3字节填充
        write_packet[pos++] = 0x00;
        write_packet[pos++] = 0x00;
        write_packet[pos++] = 0x00;
        
        // 地址（大端序）
        write_packet[pos++] = (i >> 8) & 0xFF;
        write_packet[pos++] = i & 0xFF;
        
        // 块大小（大端序）
        write_packet[pos++] = (BLOCK_SIZE >> 8) & 0xFF;
        write_packet[pos++] = BLOCK_SIZE & 0xFF;
        
        // 数据
        memcpy(&write_packet[pos], &data[i], block_len);
        pos += block_len;
        
        // 填充到固定长度
        while (pos < BLOCK_SIZE + 7) {
            write_packet[pos++] = 0x00;
        }
        
        uint8_t packet[512];
        build_write_packet_stc12(write_packet, pos, packet);
        serial_write(port, packet, get_packet_length_stc12(packet));
        
        uint8_t response[256];
        int len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0) return -1;
        
        uint8_t payload[256];
        int payload_len = parse_read_packet_stc12(response, len, payload);
        if (payload_len < 0 || payload[0] != 0x00) {
            return -2;
        }
        
        update_progress(i, BLOCK_SIZE, data_len);
    }
    
    // 发送完成命令
    uint8_t finish_packet[10];
    finish_packet[0] = 0x69;
    finish_packet[1] = 0x00;
    finish_packet[2] = 0x00;
    finish_packet[3] = 0x36;
    finish_packet[4] = 0x01;
    finish_packet[5] = (mcu_magic >> 8) & 0xFF;
    finish_packet[6] = mcu_magic & 0xFF;
    
    build_write_packet_stc12(finish_packet, 7, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    if (len < 0) return -3;
    
    int payload_len = parse_read_packet_stc12(response, len, payload);
    if (payload_len < 0 || payload[0] != 0x8D) {
        return -4;
    }
    
    return 0; // 成功
}
```

---

## STC15系列烧录算法

### 协议特点
- **校验方式**：双字节校验和
- **校验位**：偶校验（PARITY_EVEN）
- **块大小**：64字节（UART）或128字节（USB）
- **频率校准**：需要RC振荡器校准
- **编程频率**：固定22.1184 MHz
- **擦除倒计时**：0x5E（STC15A）或简化（STC15）

### 关键算法：频率校准

#### 1. STC15A频率校准算法

```c
// STC15A频率校准（两轮校准）
typedef struct {
    uint16_t trim_value;
    float final_frequency;
} trim_result_t;

trim_result_t calibrate_frequency_stc15a(serial_port_t* port, 
                                        float target_freq, 
                                        float current_freq,
                                        uint16_t freq_counter,
                                        uint8_t* trim_data) {
    trim_result_t result;
    
    // 确定目标频率计数
    float user_speed = (target_freq > 0) ? target_freq : current_freq;
    float program_speed = 22118400.0; // 固定编程频率
    
    uint32_t user_count = (uint32_t)(freq_counter * (user_speed / current_freq));
    uint32_t program_count = (uint32_t)(freq_counter * (program_speed / current_freq));
    
    // 步骤1：发送握手请求
    uint8_t handshake_req[10];
    handshake_req[0] = 0x50;
    handshake_req[1] = 0x00;
    handshake_req[2] = 0x00;
    handshake_req[3] = 0x36;
    handshake_req[4] = 0x01;
    handshake_req[5] = (mcu_magic >> 8) & 0xFF;
    handshake_req[6] = mcu_magic & 0xFF;
    
    uint8_t packet[256];
    build_write_packet_stc12(handshake_req, 7, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 1000);
    // ... 验证响应 ...
    
    // 步骤2：第一轮校准（粗调）
    uint8_t calib1_packet[256];
    uint16_t pos = 0;
    
    calib1_packet[pos++] = 0x65;
    
    // 添加校准数据（7字节）
    memcpy(&calib1_packet[pos], trim_data, 7);
    pos += 7;
    
    // 添加固定字节
    calib1_packet[pos++] = 0xFF;
    calib1_packet[pos++] = 0xFF;
    calib1_packet[pos++] = 0x06;
    calib1_packet[pos++] = 0x06;
    
    // 添加目标频率的trim挑战序列
    add_trim_sequence(calib1_packet, &pos, user_speed);
    
    // 添加编程频率的trim挑战（2个）
    calib1_packet[pos++] = 0x98;
    calib1_packet[pos++] = 0x00;
    calib1_packet[pos++] = 0x02;
    calib1_packet[pos++] = 0x00;
    calib1_packet[pos++] = 0x98;
    calib1_packet[pos++] = 0x80;
    calib1_packet[pos++] = 0x02;
    calib1_packet[pos++] = 0x00;
    
    build_write_packet_stc12(calib1_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    pulse_sync(port, 1000); // 发送同步脉冲
    
    len = serial_read_timeout(port, response, sizeof(response), 2000);
    // ... 解析响应 ...
    
    // 提取编程频率的trim值（线性插值）
    uint16_t prog_trim_a, prog_count_a, prog_trim_b, prog_count_b;
    extract_trim_pair(response, 28, &prog_trim_a, &prog_count_a);
    extract_trim_pair(response, 32, &prog_trim_b, &prog_count_b);
    
    // 计算编程频率trim值
    float m = (float)(prog_trim_b - prog_trim_a) / (prog_count_b - prog_count_a);
    float n = prog_trim_a - m * prog_count_a;
    uint16_t program_trim = (uint16_t)round(m * program_count + n);
    
    // 提取用户频率的trim范围
    uint16_t user_trim_a, user_count_a, user_trim_b, user_count_b;
    extract_trim_pair(response, 12, &user_trim_a, &user_count_a);
    extract_trim_pair(response, 16, &user_trim_b, &user_count_b);
    extract_trim_pair(response, 20, &user_trim_c, &user_count_c);
    extract_trim_pair(response, 24, &user_trim_d, &user_count_d);
    
    // 选择合适的分频范围
    if (user_count_c <= user_count && user_count_d >= user_count) {
        user_trim_a = user_trim_c;
        user_trim_b = user_trim_d;
        user_count_a = user_count_c;
        user_count_b = user_count_d;
    }
    
    // 计算第二轮校准的起始trim值
    m = (float)(user_trim_b - user_trim_a) / (user_count_b - user_count_a);
    n = user_trim_a - m * user_count_a;
    uint16_t target_trim = (uint16_t)round(m * user_count + n);
    uint16_t target_trim_start = max(min(target_trim - 5, user_trim_a), user_trim_b);
    
    // 步骤3：第二轮校准（精调）
    uint8_t calib2_packet[256];
    pos = 0;
    
    calib2_packet[pos++] = 0x65;
    memcpy(&calib2_packet[pos], trim_data, 7);
    pos += 7;
    
    calib2_packet[pos++] = 0xFF;
    calib2_packet[pos++] = 0xFF;
    calib2_packet[pos++] = 0x06;
    calib2_packet[pos++] = 0x0B; // 11个trim值
    
    // 添加11个trim挑战
    for (int i = 0; i < 11; i++) {
        uint16_t trim_val = target_trim_start + i;
        calib2_packet[pos++] = (trim_val >> 8) & 0xFF;
        calib2_packet[pos++] = trim_val & 0xFF;
        calib2_packet[pos++] = 0x02;
        calib2_packet[pos++] = 0x00;
    }
    
    build_write_packet_stc12(calib2_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    pulse_sync(port, 1000);
    
    len = serial_read_timeout(port, response, sizeof(response), 2000);
    // ... 解析响应 ...
    
    // 选择最佳trim值
    uint16_t best_trim = 0;
    uint32_t best_count = 65535;
    
    for (int i = 0; i < 11; i++) {
        uint16_t trim, count;
        extract_trim_pair(response, 12 + 4*i, &trim, &count);
        
        if (abs((int32_t)count - (int32_t)user_count) < abs((int32_t)best_count - (int32_t)user_count)) {
            best_trim = trim;
            best_count = count;
        }
    }
    
    result.trim_value = best_trim;
    result.final_frequency = (best_count / freq_counter) * current_freq;
    
    // 步骤4：切换波特率
    uint8_t baud_packet[20];
    pos = 0;
    
    baud_packet[pos++] = 0x8E;
    baud_packet[pos++] = (program_trim >> 8) & 0xFF;
    baud_packet[pos++] = program_trim & 0xFF;
    baud_packet[pos++] = 230400 / baud_transfer; // 分频比
    baud_packet[pos++] = 0xA1;
    baud_packet[pos++] = 0x64;
    baud_packet[pos++] = 0xB8;
    baud_packet[pos++] = 0x00;
    baud_packet[pos++] = get_iap_delay(program_speed);
    baud_packet[pos++] = 0x20;
    baud_packet[pos++] = 0xFF;
    baud_packet[pos++] = 0x00;
    
    build_write_packet_stc12(baud_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    serial_set_baudrate(port, baud_transfer);
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    // ... 验证响应 ...
    
    return result;
}

// 根据频率选择trim挑战序列
void add_trim_sequence(uint8_t* packet, uint16_t* pos, float frequency) {
    if (frequency < 7.5E6) {
        // 4个挑战
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    } else if (frequency < 10E6) {
        // 4个挑战
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x18; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    } else if (frequency < 15E6) {
        // 4个挑战
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    } else if (frequency < 21E6) {
        // 4个挑战
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x58; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    } else if (frequency < 31E6) {
        // 4个挑战
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0x98; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    } else {
        // 4个挑战
        packet[(*pos)++] = 0xD8; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0xD8; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0xD8; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
        packet[(*pos)++] = 0xD8; packet[(*pos)++] = 0xB4; packet[(*pos)++] = 0x02; packet[(*pos)++] = 0x00;
    }
}
```

#### 2. STC15编程Flash算法

```c
// STC15编程Flash（支持BSL 7.2+）
int program_flash_stc15(serial_port_t* port, uint8_t* data, uint32_t data_len, uint8_t bsl_version) {
    const uint16_t BLOCK_SIZE = 64;
    
    for (uint32_t i = 0; i < data_len; i += BLOCK_SIZE) {
        uint16_t block_len = (data_len - i < BLOCK_SIZE) ? (data_len - i) : BLOCK_SIZE;
        
        uint8_t write_packet[256];
        uint16_t pos = 0;
        
        // 命令字节（首块0x22，后续0x02）
        write_packet[pos++] = (i == 0) ? 0x22 : 0x02;
        
        // 地址（大端序）
        write_packet[pos++] = (i >> 8) & 0xFF;
        write_packet[pos++] = i & 0xFF;
        
        // BSL 7.2+需要魔术字
        if (bsl_version >= 0x72) {
            write_packet[pos++] = 0x5A;
            write_packet[pos++] = 0xA5;
        }
        
        // 数据
        memcpy(&write_packet[pos], &data[i], block_len);
        pos += block_len;
        
        // 填充到固定长度
        while (pos < BLOCK_SIZE + 3 + (bsl_version >= 0x72 ? 2 : 0)) {
            write_packet[pos++] = 0x00;
        }
        
        uint8_t packet[512];
        build_write_packet_stc12(write_packet, pos, packet);
        serial_write(port, packet, get_packet_length_stc12(packet));
        
        uint8_t response[256];
        int len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0) return -1;
        
        uint8_t payload[256];
        int payload_len = parse_read_packet_stc12(response, len, payload);
        if (payload_len < 2 || payload[0] != 0x02 || payload[1] != 0x54) {
            return -2;
        }
        
        update_progress(i, BLOCK_SIZE, data_len);
    }
    
    // BSL 7.2+需要完成命令
    if (bsl_version >= 0x72) {
        uint8_t finish_packet[10];
        finish_packet[0] = 0x07;
        finish_packet[1] = 0x00;
        finish_packet[2] = 0x00;
        finish_packet[3] = 0x5A;
        finish_packet[4] = 0xA5;
        
        build_write_packet_stc12(finish_packet, 5, packet);
        serial_write(port, packet, get_packet_length_stc12(packet));
        
        int len = serial_read_timeout(port, response, sizeof(response), 1000);
        if (len < 0) return -3;
        
        int payload_len = parse_read_packet_stc12(response, len, payload);
        if (payload_len < 2 || payload[0] != 0x07 || payload[1] != 0x54) {
            return -4;
        }
    }
    
    return 0; // 成功
}
```

---

## STC8系列烧录算法

### 协议特点
- **校验方式**：双字节校验和
- **校验位**：偶校验（PARITY_EVEN）
- **块大小**：64字节
- **频率校准**：使用分频器（divider）概念
- **编程频率**：固定24 MHz

### 关键算法：频率校准（STC8标准版）

```c
// STC8频率校准算法
trim_result_t calibrate_frequency_stc8(serial_port_t* port, 
                                       float target_freq, 
                                       float current_freq,
                                       uint16_t baud_handshake) {
    trim_result_t result;
    
    float user_speed = (target_freq > 0) ? target_freq : current_freq;
    uint32_t target_user_count = (uint32_t)round(user_speed / (baud_handshake / 2));
    
    // 第一轮校准：测试分频器1-5
    uint8_t calib1_packet[256];
    uint16_t pos = 0;
    
    calib1_packet[pos++] = 0x00;
    calib1_packet[pos++] = 12; // 挑战数量
    
    // 添加12个trim挑战（23*1到23*10，以及255）
    for (int i = 1; i <= 10; i++) {
        calib1_packet[pos++] = 23 * i;
        calib1_packet[pos++] = 0x00;
    }
    calib1_packet[pos++] = 255;
    calib1_packet[pos++] = 0x00;
    
    build_write_packet_stc12(calib1_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    pulse_sync(port, 0xFE, 1000);
    
    uint8_t response[256];
    int len = serial_read_timeout(port, response, sizeof(response), 2000);
    // ... 解析响应 ...
    
    // 选择合适的分频器
    uint8_t trim_divider = 0;
    uint16_t user_trim = 0;
    
    for (uint8_t divider = 1; divider <= 5; divider++) {
        user_trim = choose_range(calib1_packet, response, target_user_count * divider);
        if (user_trim != 0) {
            trim_divider = divider;
            break;
        }
    }
    
    if (trim_divider == 0) {
        return error_result(); // 校准失败
    }
    
    // 第二轮校准：精细校准（±1范围，4个分频范围）
    uint8_t calib2_packet[256];
    pos = 0;
    
    calib2_packet[pos++] = 0x00;
    calib2_packet[pos++] = 12; // 挑战数量
    
    // 4个分频范围，每个范围3个trim值（±1）
    for (uint8_t range = 0; range < 4; range++) {
        for (int i = -1; i <= 1; i++) {
            calib2_packet[pos++] = (user_trim + i) & 0xFF;
            calib2_packet[pos++] = range;
        }
    }
    
    build_write_packet_stc12(calib2_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    delay_ms(100);
    pulse_sync(port, 0xFE, 1000);
    
    len = serial_read_timeout(port, response, sizeof(response), 2000);
    // ... 解析响应 ...
    
    // 选择最佳trim值
    uint16_t best_trim = 0;
    uint32_t best_count = 0xFFFFFFFF;
    
    for (int i = 0; i < 12; i++) {
        uint16_t count;
        extract_count(response, 2 + 2*i, &count);
        
        if (abs((int32_t)count - (int32_t)target_user_count) < abs((int32_t)best_count - (int32_t)target_user_count)) {
            best_trim = calib2_packet[2 + 2*i];
            best_count = count;
        }
    }
    
    result.trim_value = best_trim;
    result.trim_divider = trim_divider;
    result.final_frequency = best_count * (baud_handshake / 2) / trim_divider;
    
    // 切换波特率
    uint8_t baud_packet[20];
    pos = 0;
    
    baud_packet[pos++] = 0x01;
    baud_packet[pos++] = 0x00;
    baud_packet[pos++] = 0x00;
    
    uint16_t bauds = baud_transfer * 4;
    uint16_t brt_val = (uint16_t)round(65536 - 24E6 / bauds);
    baud_packet[pos++] = (brt_val >> 8) & 0xFF;
    baud_packet[pos++] = brt_val & 0xFF;
    
    // trim值（注意顺序：range在前，trim在后）
    baud_packet[pos++] = best_trim_range;
    baud_packet[pos++] = best_trim;
    
    baud_packet[pos++] = get_iap_delay(24E6);
    
    build_write_packet_stc12(baud_packet, pos, packet);
    serial_write(port, packet, get_packet_length_stc12(packet));
    
    len = serial_read_timeout(port, response, sizeof(response), 1000);
    // ... 验证响应 ...
    
    serial_set_baudrate(port, baud_transfer);
    
    return result;
}
```

### STC8d/STC8g特殊算法

```c
// STC8d第一轮校准包格式
void build_calib1_packet_stc8d(uint8_t* packet, uint16_t* pos) {
    packet[(*pos)++] = 0x00;
    packet[(*pos)++] = 0x08; // 挑战数量
    
    // 4组trim挑战
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x00;
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x10; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x10;
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x20; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x20;
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x30; packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x30;
}

// STC8d第二轮校准（±6范围）
void build_calib2_packet_stc8d(uint8_t* packet, uint16_t* pos, uint16_t trim_start, uint8_t trim_range) {
    packet[(*pos)++] = 0x00;
    packet[(*pos)++] = 0x0C; // 12个挑战
    
    // 12个trim值（±6范围）
    for (int i = -6; i < 6; i++) {
        packet[(*pos)++] = (trim_start + i) & 0xFF;
        packet[(*pos)++] = trim_range;
    }
}

// STC8g第一轮校准（需要epilogue）
void build_calib1_packet_stc8g(uint8_t* packet, uint16_t* pos) {
    packet[(*pos)++] = 0x00;
    packet[(*pos)++] = 0x05; // 挑战数量
    
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x00;
    packet[(*pos)++] = 0x00; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x80; packet[(*pos)++] = 0x80;
    packet[(*pos)++] = 0xFF; packet[(*pos)++] = 0x00;
    
    // 需要添加12字节的epilogue（0x66）
    // 在write_packet时指定epilogue_len = 12
}
```

---

## STC32系列烧录算法

### 协议特点
- **协议基础**：基于STC8d协议
- **架构**：MCS-251（32位）
- **地址空间**：CODE和EEPROM分离
- **CODE起始**：0xFF0000
- **EEPROM起始**：0xFE0000

### 关键算法

```c
// STC32地址处理
void setup_address_stc32(uint32_t address, uint8_t* packet, uint16_t* pos) {
    if (address >= 0xFF0000) {
        // CODE区域
        uint32_t code_addr = address - 0xFF0000;
        packet[(*pos)++] = (code_addr >> 8) & 0xFF;
        packet[(*pos)++] = code_addr & 0xFF;
    } else if (address >= 0xFE0000) {
        // EEPROM区域
        uint32_t eeprom_addr = address - 0xFE0000;
        packet[(*pos)++] = (eeprom_addr >> 8) & 0xFF;
        packet[(*pos)++] = eeprom_addr & 0xFF;
    }
}

// STC32编程Flash（支持CODE/EEPROM分离）
int program_flash_stc32(serial_port_t* port, uint8_t* data, uint32_t data_len, 
                        uint32_t code_size, uint32_t eeprom_size) {
    // CODE区域编程
    if (code_size > 0) {
        uint32_t code_start = 0xFF0000;
        for (uint32_t i = 0; i < code_size; i += BLOCK_SIZE) {
            program_block_stc32(port, &data[i], code_start + i, BLOCK_SIZE);
        }
    }
    
    // EEPROM区域编程
    if (eeprom_size > 0) {
        uint32_t eeprom_start = 0xFE0000;
        uint32_t eeprom_offset = code_size;
        for (uint32_t i = 0; i < eeprom_size; i += BLOCK_SIZE) {
            program_block_stc32(port, &data[eeprom_offset + i], eeprom_start + i, BLOCK_SIZE);
        }
    }
    
    return 0;
}
```

---

## USB协议烧录算法

### 协议特点
- **通信方式**：USB控制传输
- **VID/PID**：0x5354/0x4312
- **块大小**：128字节
- **校验方式**：每7字节数据+1字节校验和

### 关键算法

```c
// USB数据包构建（分块传输）
void build_usb_packet(uint8_t* data, uint16_t data_len, uint8_t* output) {
    uint16_t pos = 0;
    uint16_t i = 0;
    
    while (i < data_len) {
        // 每7字节为一组
        uint8_t chunk_len = (data_len - i < 7) ? (data_len - i) : 7;
        
        // 数据
        memcpy(&output[pos], &data[i], chunk_len);
        pos += chunk_len;
        i += chunk_len;
        
        // 校验和（减法校验）
        uint8_t checksum = 0;
        for (int j = 0; j < chunk_len; j++) {
            checksum = (checksum - output[pos - chunk_len + j]) & 0xFF;
        }
        output[pos++] = checksum;
    }
}

// USB读取数据包
int read_usb_packet(usb_device_t* dev, uint8_t* output) {
    uint8_t packet[132];
    
    // USB控制传输读取
    int len = usb_control_transfer(dev, 
                                   USB_REQ_TYPE_VENDOR | USB_REQ_IN,
                                   0, 0, 0, 
                                   packet, sizeof(packet), 1000);
    
    if (len < 5 || packet[0] != 0x46 || packet[1] != 0xB9) {
        return -1; // 帧起始错误
    }
    
    uint8_t data_len = packet[2];
    if (data_len > len - 3) {
        return -2; // 长度错误
    }
    
    // 验证校验和（减法校验）
    uint8_t checksum = 0;
    for (int i = 2; i < len - 1; i++) {
        checksum = (checksum - packet[i]) & 0xFF;
    }
    
    if (checksum != packet[len - 1]) {
        return -3; // 校验和错误
    }
    
    // 提取载荷
    memcpy(output, &packet[3], data_len);
    
    return data_len;
}

// USB握手
int handshake_usb(usb_device_t* dev) {
    // 步骤1：握手
    uint8_t handshake_data[1] = {0x03};
    uint8_t packet[16];
    build_usb_packet(handshake_data, 1, packet);
    
    usb_control_transfer(dev, USB_REQ_TYPE_VENDOR | USB_REQ_OUT,
                        0x01, 0, 0, packet, get_usb_packet_length(packet), 1000);
    
    uint8_t response[256];
    int len = read_usb_packet(dev, response);
    if (len < 0 || response[0] != 0x01) {
        return -1;
    }
    
    // 步骤2：解锁MCU
    uint8_t unlock_data[1] = {0x00};
    build_usb_packet(unlock_data, 1, packet);
    
    usb_control_transfer(dev, USB_REQ_TYPE_VENDOR | USB_REQ_OUT,
                        0x05, 0xA55A, 0, packet, get_usb_packet_length(packet), 1000);
    
    len = read_usb_packet(dev, response);
    if (len < 0) return -2;
    
    if (response[0] == 0x0F) {
        return -3; // MCU已锁定
    }
    if (response[0] != 0x05) {
        return -4;
    }
    
    return 0; // 成功
}

// USB编程Flash
int program_flash_usb(usb_device_t* dev, uint8_t* data, uint32_t data_len) {
    const uint16_t BLOCK_SIZE = 128;
    
    for (uint32_t i = 0; i < data_len; i += BLOCK_SIZE) {
        uint16_t block_len = (data_len - i < BLOCK_SIZE) ? (data_len - i) : BLOCK_SIZE;
        
        uint8_t block_data[BLOCK_SIZE];
        memcpy(block_data, &data[i], block_len);
        
        // 填充到128字节
        while (block_len < BLOCK_SIZE) {
            block_data[block_len++] = 0x00;
        }
        
        // 构建USB数据包
        uint8_t packet[256];
        build_usb_packet(block_data, BLOCK_SIZE, packet);
        
        // 发送（首块0x22，后续0x02）
        uint8_t request = (i == 0) ? 0x22 : 0x02;
        usb_control_transfer(dev, USB_REQ_TYPE_VENDOR | USB_REQ_OUT,
                            request, 0xA55A, i, packet, get_usb_packet_length(packet), 1000);
        
        delay_ms(100);
        
        // 读取响应
        uint8_t response[256];
        int len = read_usb_packet(dev, response);
        if (len < 2 || response[0] != 0x02 || response[1] != 0x54) {
            return -1;
        }
        
        update_progress(i, BLOCK_SIZE, data_len);
    }
    
    return 0; // 成功
}
```

---

## 算法对比总结

### 数据包格式对比

| 型号 | 校验和 | 校验位 | BRT宽度 | 块大小 | 特殊特性 |
|------|--------|--------|---------|--------|----------|
| STC89 | 单字节 | 无 | 16位 | 128 | 简单协议 |
| STC89A | 双字节 | 偶校验 | 16位 | 128 | 握手后切换校验 |
| STC12 | 双字节 | 偶校验 | 8位 | 128 | 擦除倒计时 |
| STC15A | 双字节 | 偶校验 | - | 64 | 频率校准（两轮） |
| STC15 | 双字节 | 偶校验 | - | 64 | 频率校准（简化） |
| STC8 | 双字节 | 偶校验 | - | 64 | 分频器校准 |
| STC8d | 双字节 | 偶校验 | - | 64 | 特殊校准包格式 |
| STC8g | 双字节 | 偶校验 | - | 64 | Epilogue填充 |
| STC32 | 双字节 | 偶校验 | - | 64 | MCS-251地址 |
| USB15 | 单字节/块 | - | - | 128 | USB控制传输 |

### 关键算法差异

1. **校验和计算**：
   - STC89：`sum(packet[2:-2]) & 0xFF`
   - STC89A/12+：`sum(packet[2:-3]) & 0xFFFF`
   - USB：每7字节一组，`reduce(lambda x, y: x - y, data, 0) & 0xFF`

2. **波特率计算**：
   - STC89：`65536 - clock / (baud * sample_rate)`，sample_rate=16/32
   - STC12：`256 - clock / (baud * 16)`
   - STC15+：固定编程频率，通过trim值调整

3. **频率校准**：
   - STC89/12：不需要校准
   - STC15A：两轮校准，线性插值
   - STC15：两轮校准，简化版
   - STC8：分频器概念，两轮校准

4. **擦除命令**：
   - STC89：`0x84` + 块数
   - STC89A：`0x03`（全擦除）
   - STC12：`0x84` + 倒计时序列
   - STC15：`0x03` + 擦除类型

5. **编程命令**：
   - STC89：统一`0x02`命令
   - STC89A：首块`0x22`，后续`0x02`
   - STC12：统一`0x02`命令，完成后`0x69`
   - STC15：首块`0x22`，后续`0x02`，BSL7.2+需要`0x07`完成

### 实现建议

1. **模块化设计**：将各型号算法封装为独立模块
2. **统一接口**：定义统一的协议接口，便于切换
3. **配置驱动**：使用配置表驱动不同型号的处理
4. **错误处理**：统一的错误码和处理机制
5. **性能优化**：使用DMA、批量处理等技术

---

**文档版本**：v1.0  
**最后更新**：2024年  
**基于**：stcgal源码分析

