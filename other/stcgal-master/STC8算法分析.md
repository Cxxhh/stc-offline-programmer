# STC8 系列单片机烧录算法分析

## 概述

本文档详细分析了 stcgal 项目中 STC8 系列单片机的烧录算法实现，为移植到 STM32 平台提供参考。

## 1. 协议架构

### 1.1 基础协议类

STC8 协议继承自 `Stc15Protocol`，主要实现在 `Stc8Protocol` 类中：

```1993:2144:other/stcgal-master/stcgal/protocols.py
class Stc8Protocol(Stc15Protocol):
    """Protocol handler for STC8 series"""

    def __init__(self, port, handshake, baud, trim):
        Stc15Protocol.__init__(self, port, handshake, baud, trim)
        self.trim_divider = None
        self.reference_voltage = None
        self.mfg_date = ()
```

### 1.2 数据包格式

#### 数据包结构
- **帧头**: `0x46 0xB9` (PACKET_START)
- **方向标识**: 
  - `0x68` (PACKET_MCU) - MCU 发送
  - `0x6A` (PACKET_HOST) - 主机发送
- **长度**: 2字节大端序，包含整个数据包长度
- **数据载荷**: 可变长度
- **校验和**: 2字节大端序（STC8 使用偶校验）
- **帧尾**: `0x16` (PACKET_END)

#### 校验和计算
```python
# 对于 STC8，校验和是 16 位
calc_csum = sum(packet[2:-3]) & 0xffff
```

## 2. 连接与初始化流程

### 2.1 连接流程

1. **串口初始化**
   - 波特率：握手阶段使用 `baud_handshake`（通常 2400 baud）
   - 校验位：偶校验 (PARITY_EVEN)
   - 超时：0.5秒（快速检测）

2. **同步脉冲**
   - 发送 `0x7F` 字节进行同步
   - 等待 MCU 响应状态包

3. **读取状态包**
   - 状态包格式：`0x50` + 状态数据
   - 如果收到 `0x80`，需要重新确认

### 2.2 状态包解析

```2011:2037:other/stcgal-master/stcgal/protocols.py
    def initialize_status(self, packet):
        """Decode status packet and store basic MCU info"""

        if len(packet) < 39:
            raise StcProtocolException("invalid status packet")

        self.mcu_clock_hz, = struct.unpack(">I", packet[1:5])
        self.external_clock = False
        # all ones means no calibration
        # new chips are shipped without any calibration
        # XXX: somehow check if that still holds
        if self.mcu_clock_hz == 0xffffffff: self.mcu_clock_hz = 0

        # wakeup timer factory value
        self.wakeup_freq, = struct.unpack(">H", packet[23:25])
        self.reference_voltage, = struct.unpack(">H", packet[35:37])
        self.mfg_date = (
            2000 + Utils.decode_packed_bcd(packet[37]),
            Utils.decode_packed_bcd(packet[38]),
            Utils.decode_packed_bcd(packet[39])
        )

        bl_version, bl_stepping = struct.unpack("BB", packet[17:19])
        bl_minor = packet[22] & 0x0f
        self.mcu_bsl_version = "%d.%d.%d%s" % (bl_version >> 4, bl_version & 0x0f,
                                               bl_minor, chr(bl_stepping))
        self.bsl_version = bl_version
```

**状态包关键字段**：
- 字节 1-4: MCU 时钟频率 (32位大端)
- 字节 17-18: BSL 版本号
- 字节 23-24: 唤醒定时器频率
- 字节 35-36: 参考电压
- 字节 37-39: 制造日期 (BCD 编码)

## 3. 频率校准算法

### 3.1 STC8 标准校准流程

STC8 使用两轮校准来确定最佳频率调整值：

```2048:2120:other/stcgal-master/stcgal/protocols.py
    def calibrate(self):
        """Calibrate selected user frequency frequency and switch to selected baudrate."""

        # handle uncalibrated chips
        if self.mcu_clock_hz == 0 and self.trim_frequency <= 0:
            raise StcProtocolException("uncalibrated, please provide a trim value")

        # determine target counter
        user_speed = self.trim_frequency
        if user_speed <= 0: user_speed = self.mcu_clock_hz
        target_user_count = round(user_speed / (self.baud_handshake/2))

        # calibration, round 1
        print("Trimming frequency: ", end="")
        sys.stdout.flush()
        packet = bytes([0x00])
        packet += struct.pack(">B", 12)
        packet += bytes([0x00, 0x00, 23*1, 0x00, 23*2, 0x00])
        packet += bytes([23*3, 0x00, 23*4, 0x00, 23*5, 0x00])
        packet += bytes([23*6, 0x00, 23*7, 0x00, 23*8, 0x00])
        packet += bytes([23*9, 0x00, 23*10, 0x00, 255, 0x00])
        self.write_packet(packet)
        self.pulse(b"\xfe", timeout=1.0)
        response = self.read_packet()
        if len(response) < 2 or response[0] != 0x00:
            raise StcProtocolException("incorrect magic in handshake packet")

        # select ranges and trim values
        for divider in (1, 2, 3, 4, 5):
            user_trim = self.choose_range(packet, response, target_user_count * divider)
            if user_trim is not None:
                self.trim_divider = divider
                break
        if user_trim is None:
            raise StcProtocolException("frequency trimming unsuccessful")

        # calibration, round 2
        packet = bytes([0x00])
        packet += struct.pack(">B", 12)
        for i in range(user_trim[0] - 1, user_trim[0] + 2):
            packet += bytes([i & 0xff, 0x00])
        for i in range(user_trim[0] - 1, user_trim[0] + 2):
            packet += bytes([i & 0xff, 0x01])
        for i in range(user_trim[0] - 1, user_trim[0] + 2):
            packet += bytes([i & 0xff, 0x02])
        for i in range(user_trim[0] - 1, user_trim[0] + 2):
            packet += bytes([i & 0xff, 0x03])
        self.write_packet(packet)
        self.pulse(b"\xfe", timeout=1.0)
        response = self.read_packet()
        if len(response) < 2 or response[0] != 0x00:
            raise StcProtocolException("incorrect magic in handshake packet")

        # select final values
        user_trim, user_count = self.choose_trim(packet, response, target_user_count)
        self.trim_value = user_trim
        self.trim_frequency = round(user_count * (self.baud_handshake / 2) / self.trim_divider)
        print("%.03f MHz" % (self.trim_frequency / 1E6))

        # switch to programming frequency
        print("Switching to %d baud: " % self.baud_transfer, end="")
        sys.stdout.flush()
        packet = bytes([0x01, 0x00, 0x00])
        bauds = self.baud_transfer * 4
        packet += struct.pack(">H", round(65536 - 24E6 / bauds))
        packet += bytes([user_trim[1], user_trim[0]])
        iap_wait = self.get_iap_delay(24E6)
        packet += bytes([iap_wait])
        self.write_packet(packet)
        response = self.read_packet()
        if len(response) < 1 or response[0] != 0x01:
            raise StcProtocolException("incorrect magic in handshake packet")
        self.ser.baudrate = self.baud_transfer
```

### 3.2 校准关键点

1. **第一轮校准**：
   - 命令：`0x00` + 长度 `0x0C` (12)
   - 发送 12 个测试点：`[0, 23, 46, 69, 92, 115, 138, 161, 184, 207, 230, 255]`
   - MCU 返回每个测试点对应的频率计数值
   - 根据目标频率选择合适的分频器和范围

2. **第二轮校准**：
   - 在第一轮选定的范围内，发送更精细的测试点
   - 每个 trim 值测试 4 个不同的 fine adjustment (0x00, 0x01, 0x02, 0x03)
   - 选择最接近目标频率的 trim 值

3. **波特率切换**：
   - 命令：`0x01 0x00 0x00`
   - 波特率计算：`BRT = 65536 - (24MHz / (baudrate * 4))`
   - 固定使用 24MHz 作为编程频率
   - 包含 trim 值和 IAP 等待时间

### 3.3 分频器机制

STC8 支持时钟分频（1-5倍），用于支持较低的工作频率：
- 校准总是在 ~20-30 MHz 范围内进行
- 较低频率通过分频器实现
- `trim_divider` 记录使用的分频倍数

## 4. Flash 擦除算法

STC8 继承 STC15 的擦除算法：

```1727:1756:other/stcgal-master/stcgal/protocols.py
    def erase_flash(self, erase_size, flash_size):
        """Erase the MCU's flash memory.

        Erase the flash memory with a block-erase command.
        Note that this protocol always seems to erase everything.
        """

        print("Erasing flash: ", end="")
        sys.stdout.flush()
        packet = bytes([0x03])
        if erase_size <= flash_size:
           # erase flash only
           packet += bytes([0x00])
        else:
           # erase flash and eeprom
           packet += bytes([0x01])
        if self.bsl_version >= 0x72:
            packet += bytes([0x00, 0x5a, 0xa5])
        self.write_packet(packet)
        response = self.read_packet()
        if len(response) < 1 or response[0] != 0x03:
            raise StcProtocolException("incorrect magic in handshake packet")
        print("done")

        if len(response) >= 8:
            self.uid = response[1:8]

        # we should have a UID at this point
        if not self.uid:
            raise StcProtocolException("UID is missing")
```

**擦除命令格式**：
- 命令字节：`0x03`
- 擦除类型：`0x00` (仅 Flash) 或 `0x01` (Flash + EEPROM)
- BSL 7.2+ 需要额外发送：`0x00 0x5A 0xA5`
- 响应：`0x03` + UID (7字节)

## 5. Flash 编程算法

```1758:1784:other/stcgal-master/stcgal/protocols.py
    def program_flash(self, data):
        """Program the MCU's flash memory."""

        for i in range(0, len(data), self.PROGRAM_BLOCKSIZE):
            packet = bytes([0x22]) if i == 0 else bytes([0x02])
            packet += struct.pack(">H", i)
            if self.bsl_version >= 0x72:
                packet += bytes([0x5a, 0xa5])
            packet += data[i:i+self.PROGRAM_BLOCKSIZE]
            while len(packet) < self.PROGRAM_BLOCKSIZE + 3: packet += b"\x00"
            self.write_packet(packet)
            response = self.read_packet()
            if len(response) < 2 or response[0] != 0x02 or response[1] != 0x54:
                raise StcProtocolException("incorrect magic in write packet")
            self.progress_cb(i, self.PROGRAM_BLOCKSIZE, len(data))
        self.progress_cb(len(data), self.PROGRAM_BLOCKSIZE, len(data))

        # BSL 7.2+ needs a write finish packet according to dumps
        if self.bsl_version >= 0x72:
            print("Finishing write: ", end="")
            sys.stdout.flush()
            packet = bytes([0x07, 0x00, 0x00, 0x5a, 0xa5])
            self.write_packet(packet)
            response = self.read_packet()
            if len(response) < 2 or response[0] != 0x07 or response[1] != 0x54:
                raise StcProtocolException("incorrect magic in finish packet")
            print("done")
```

**编程命令格式**：
- **首块**：`0x22` + 地址(2字节) + [0x5A 0xA5] + 数据(64字节)
- **后续块**：`0x02` + 地址(2字节) + [0x5A 0xA5] + 数据(64字节)
- **块大小**：64 字节 (PROGRAM_BLOCKSIZE)
- **响应**：`0x02 0x54` (或 `0x07 0x54` 对于完成包)
- **完成包** (BSL 7.2+)：`0x07 0x00 0x00 0x5A 0xA5`

## 6. 选项配置算法

### 6.1 选项数据结构

```2122:2135:other/stcgal-master/stcgal/protocols.py
    def build_options(self):
        """Build a packet of option data from the current configuration."""

        msr = self.options.get_msr()
        packet = 40 * bytearray([0xff])
        packet[3] = 0
        packet[6] = 0
        packet[22] = 0
        packet[24:28] = struct.pack(">I", self.trim_frequency)
        packet[28:30] = self.trim_value
        packet[30] = self.trim_divider
        packet[32] = msr[0]
        packet[36:40] = msr[1:5]
        return bytes(packet)
```

**选项包格式** (40字节)：
- 字节 3, 6, 22: 固定为 0
- 字节 24-27: 校准后的频率 (32位大端)
- 字节 28-29: trim 值 (2字节)
- 字节 30: 分频器值
- 字节 32: MCS0 (选项字节0)
- 字节 36-39: MCS1-4 (选项字节1-4)

### 6.2 选项配置命令

```1811:1825:other/stcgal-master/stcgal/protocols.py
    def program_options(self):
        print("Setting options: ", end="")
        sys.stdout.flush()

        packet = bytes([0x04, 0x00, 0x00])
        if self.bsl_version >= 0x72:
            packet += bytes([0x5a, 0xa5])
        packet += self.build_options()
        self.write_packet(packet)
        response = self.read_packet()
        if len(response) < 2 or response[0] != 0x04 or response[1] != 0x54:
            raise StcProtocolException("incorrect magic in option packet")
        print("done")

        print("Target UID: %s" % Utils.hexstr(self.uid))
```

**选项命令格式**：
- 命令：`0x04 0x00 0x00` + [0x5A 0xA5] + 选项数据(40字节)
- 响应：`0x04 0x54`

### 6.3 STC8 选项字节定义

参考 `Stc8Option` 类：

```626:649:other/stcgal-master/stcgal/options.py
class Stc8Option(BaseOption):
    def __init__(self, msr):
        super().__init__()
        assert len(msr) >= 5
        self.msr = bytearray(msr)

        self.options = (
            ("reset_pin_enabled", self.get_reset_pin_enabled, self.set_reset_pin_enabled),
            ("clock_gain", self.get_clock_gain, self.set_clock_gain),
            ("watchdog_por_enabled", self.get_watchdog, self.set_watchdog),
            ("watchdog_stop_idle", self.get_watchdog_idle, self.set_watchdog_idle),
            ("watchdog_prescale", self.get_watchdog_prescale, self.set_watchdog_prescale),
            ("low_voltage_reset", self.get_lvrs, self.set_lvrs),
            ("low_voltage_threshold", self.get_low_voltage, self.set_low_voltage),
            ("eeprom_erase_enabled", self.get_ee_erase, self.set_ee_erase),
            ("bsl_pindetect_enabled", self.get_pindetect, self.set_pindetect),
            ("por_reset_delay", self.get_por_delay, self.set_por_delay),
            ("rstout_por_state", self.get_p20_state, self.set_p20_state),
            ("uart1_remap", self.get_uart1_remap, self.set_uart1_remap),
            ("uart2_passthrough", self.get_uart_passthrough, self.set_uart_passthrough),
            ("uart2_pin_mode", self.get_uart_pin_mode, self.set_uart_pin_mode),
            ("epwm_open_drain", self.get_epwm_pp, self.set_epwm_pp),
            ("program_eeprom_split", self.get_flash_split, self.set_flash_split),
        )
```

**MCS 字节位定义** (根据文档)：
- **MCS0**: BSL 使能、EEPROM 擦除使能等
- **MCS1**: 时钟增益、EPWM 配置、UART 重映射等
- **MCS2**: 低电压检测阈值、复位引脚使能等
- **MCS3**: 看门狗配置
- **MCS4**: EEPROM/代码空间分割

## 7. 断开连接

```2137:2144:other/stcgal-master/stcgal/protocols.py
    def disconnect(self):
        """Disconnect from MCU"""

        # reset mcu
        packet = bytes([0xff])
        self.write_packet(packet)
        self.ser.close()
        print("Disconnected!")
```

**断开命令**：`0xFF` - 复位 MCU 并断开连接

## 8. STC8 变种协议

### 8.1 Stc8dProtocol (STC8A8K64D4)

主要区别：
- 不同的校准挑战数据包格式
- 不同的 `choose_range` 和 `choose_trim` 实现
- 支持 MCS251 模式（扩展地址空间）

### 8.2 Stc8gProtocol (STC8G 系列)

主要区别：
- 第一轮校准使用不同的挑战数据：`0x00 0x05` + 特定测试点
- 需要额外的 epilogue 字节（12 和 19 字节）

## 9. 移植到 STM32 的关键点

### 9.1 数据包处理
- 实现帧头/帧尾检测
- 实现 16 位校验和计算
- 处理偶校验位

### 9.2 串口通信
- 支持动态波特率切换
- 实现脉冲同步机制
- 处理超时和错误重试

### 9.3 频率校准
- 实现两轮校准算法
- 处理分频器选择
- 计算最佳 trim 值

### 9.4 Flash 操作
- 实现块擦除
- 实现分块编程（64字节块）
- 处理地址对齐

### 9.5 选项配置
- 构建 40 字节选项包
- 处理 MCS 字节位域

## 10. 协议流程图

```
连接流程：
1. 初始化串口 (2400 baud, 偶校验)
2. 发送同步脉冲 (0x7F)
3. 读取状态包 (0x50)
4. 解析 MCU 信息

校准流程：
1. 发送第一轮校准挑战 (0x00 + 12个测试点)
2. 接收响应，选择分频器和范围
3. 发送第二轮校准挑战 (0x00 + 精细测试点)
4. 选择最佳 trim 值
5. 切换波特率 (0x01 + BRT + trim)

烧录流程：
1. 擦除 Flash (0x03)
2. 分块编程 (0x22/0x02 + 地址 + 数据)
3. 完成编程 (0x07, BSL 7.2+)
4. 配置选项 (0x04 + 选项数据)
5. 断开连接 (0xFF)
```

## 11. 参考资料

- `stcgal/protocols.py` - 协议实现
- `stcgal/options.py` - 选项配置
- `doc/reverse-engineering/stc8-protocol.txt` - 协议逆向文档
- `doc/reverse-engineering/stc8-options.txt` - 选项字节定义

## 12. 注意事项

1. **BSL 版本兼容性**：不同 BSL 版本可能有细微差异，需要检查 `bsl_version`
2. **固定编程频率**：STC8 使用固定的 24MHz 进行编程，不受用户频率影响
3. **分频器机制**：低频率通过分频器实现，校准始终在 ~20-30MHz 范围
4. **数据对齐**：Flash 编程需要 64 字节对齐
5. **超时处理**：各操作需要适当的超时机制


