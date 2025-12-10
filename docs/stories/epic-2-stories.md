# Epic 2: ISP握手协议与MCU识别 - 用户故事

## 史诗目标

实现STC ISP握手协议的完整流程，包括握手信号发送、目标响应解析、MCU型号识别。建立协议帧解析框架，支持不同STC系列的协议差异。

## 关联需求

| 需求ID | 描述 | 优先级 |
|--------|------|--------|
| FR1 | 系统应支持通过UART与STC目标单片机通信，实现ISP握手协议 | P0 |
| FR2 | 系统应能通过Magic码自动识别目标STC单片机型号 | P0 |
| FR7 | 系统应支持动态波特率切换，握手阶段使用低速（2400bps） | P0 |
| NFR1 | 握手响应时间应小于3秒 | P0 |

---

## Story S2.1: ISP协议帧结构定义

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S2.1 |
| **优先级** | 🔴 P0 |
| **估算点数** | 3 |
| **前置依赖** | S1.3 |

### 用户故事

**As a** 开发者,  
**I want** 定义ISP协议的帧结构和常量,  
**so that** 协议解析有统一的数据结构支撑。

### 验收标准

1. ✅ 在 `stc_protocol_def.h` 中定义帧头、帧尾常量
2. ✅ 定义命令码枚举（握手、擦除、编程、校验等）
3. ✅ 定义响应状态码枚举
4. ✅ 定义协议帧结构体（包含帧头、长度、命令、数据、校验和）
5. ✅ 定义MCU信息响应结构体
6. ✅ 添加必要的宏定义（超时时间、重试次数等）

### 技术说明

- 参考 stcgal 开源项目的协议逆向分析
- STC ISP 协议帧格式：
  ```
  | 帧头(2) | 方向(1) | 长度(2) | 命令(1) | 数据(N) | 校验和(1) | 帧尾(1) |
  | 0x46 0xB9 | 0x6A/0x68 | LEN_H LEN_L | CMD | DATA... | SUM | 0x16 |
  ```
- 不同 STC 系列的命令码可能有差异

### 接口定义

```c
/* Middlewares/stc_isp/stc_protocol_def.h */

/* 帧定界符 */
#define STC_FRAME_HEAD1         0x46
#define STC_FRAME_HEAD2         0xB9
#define STC_FRAME_TAIL          0x16

/* 方向标识 */
#define STC_DIR_HOST_TO_MCU     0x6A
#define STC_DIR_MCU_TO_HOST     0x68

/* 命令码枚举 */
typedef enum {
    STC_CMD_HANDSHAKE   = 0x00,
    STC_CMD_ERASE       = 0x84,
    STC_CMD_PROGRAM     = 0x80,
    STC_CMD_VERIFY      = 0x83,
    // ...
} stc_cmd_t;

/* 协议帧结构 */
typedef struct {
    uint8_t  direction;
    uint16_t length;
    uint8_t  command;
    uint8_t  data[256];
    uint8_t  checksum;
} stc_frame_t;
```

### 完成定义（DoD）

- [ ] 所有协议常量和结构体定义完成
- [ ] 头文件编译无错误
- [ ] 添加必要的注释说明
- [ ] 代码通过代码规范检查

---

## Story S2.2: 握手信号发送与检测

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S2.2 |
| **优先级** | 🔴 P0 |
| **估算点数** | 5 |
| **前置依赖** | S2.1 |

### 用户故事

**As a** 开发者,  
**I want** 实现握手信号的发送和响应检测,  
**so that** 能够触发STC目标MCU进入ISP模式。

### 验收标准

1. ✅ 实现 `stc_send_handshake()` 函数，以 2400bps 发送握手序列
2. ✅ 实现握手响应检测，识别有效的 ISP 响应
3. ✅ 支持 STC89 系列和 STC15/8 系列的不同握手序列
4. ✅ 实现超时检测机制（默认 3 秒超时）
5. ✅ 实现握手重试机制（默认 3 次重试）
6. ✅ 在目标MCU上电时能成功触发握手响应

### 技术说明

- 握手流程：
  1. 配置串口为 2400bps 8N1（STC89）或 8E1（STC15/8）
  2. 持续发送 0x7F 握手字节
  3. 等待目标 MCU 上电复位
  4. 检测 MCU 返回的握手响应帧
- STC89 系列握手序列与 STC15/8 系列有差异
- 握手成功后，MCU 会返回包含设备信息的响应帧

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 握手配置 */
typedef struct {
    uint32_t timeout_ms;        /* 超时时间，默认 3000ms */
    uint8_t  retry_count;       /* 重试次数，默认 3 */
    uint8_t  mcu_series;        /* MCU 系列 (89/15/8) */
} stc_handshake_config_t;

/* 握手结果 */
typedef struct {
    bool     success;
    uint8_t  response[64];
    uint16_t response_len;
} stc_handshake_result_t;

int stc_send_handshake(serial_dev_t *serial, const stc_handshake_config_t *config, stc_handshake_result_t *result);
```

### 完成定义（DoD）

- [ ] 握手函数实现完成
- [ ] 使用实际 STC 芯片测试握手成功
- [ ] 超时和重试机制工作正常
- [ ] 握手响应时间 < 3 秒
- [ ] 代码通过代码规范检查

---

## Story S2.3: 目标MCU信息解析

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S2.3 |
| **优先级** | 🔴 P0 |
| **估算点数** | 5 |
| **前置依赖** | S2.2 |

### 用户故事

**As a** 开发者,  
**I want** 解析目标MCU的响应信息,  
**so that** 能够获取MCU型号、Flash大小等关键参数。

### 验收标准

1. ✅ 实现 `stc_parse_mcu_info()` 函数，解析握手响应数据
2. ✅ 提取 Magic 码并通过数据库匹配 MCU 型号
3. ✅ 提取并存储 MCU 的 Flash 大小、EEPROM 大小信息
4. ✅ 提取并存储 MCU 的 UID（唯一标识符）
5. ✅ 正确识别 MCU 所属系列（89/10/11/12/15/8）
6. ✅ 对未知 Magic 码返回适当的错误信息

### 技术说明

- MCU 信息响应帧格式（以 STC15 为例）：
  ```
  | 帧头 | 方向 | 长度 | 命令 | Magic(2) | UID(7) | Flash大小 | ... | 校验和 | 帧尾 |
  ```
- Magic 码是 2 字节的设备标识，用于匹配 MCU 数据库
- MCU 数据库位于 `Middlewares/stc_isp/stc_mcu_database.c`（1140+ 型号）
- UID 是 7 字节的唯一标识符

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* MCU 信息结构 */
typedef struct {
    uint16_t magic;                     /* Magic 码 */
    const stc_mcu_model_t *model;       /* MCU 型号（数据库匹配） */
    uint8_t  uid[7];                    /* 唯一标识符 */
    uint32_t flash_size;                /* Flash 大小 (bytes) */
    uint32_t eeprom_size;               /* EEPROM 大小 (bytes) */
    uint8_t  series;                    /* MCU 系列 */
} stc_mcu_info_t;

int stc_parse_mcu_info(const uint8_t *response, uint16_t len, stc_mcu_info_t *info);
const char *stc_get_series_name(uint8_t series);
```

### 完成定义（DoD）

- [ ] MCU 信息解析函数实现完成
- [ ] 使用多款 STC 芯片测试，型号识别正确
- [ ] 未知 Magic 码返回 `ISP_ERR_UNSUPPORTED`
- [ ] UID 提取正确（与官方工具对比）
- [ ] 代码通过代码规范检查

---

## Story S2.4: 波特率协商实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S2.4 |
| **优先级** | 🔴 P0 |
| **估算点数** | 5 |
| **前置依赖** | S2.3 |

### 用户故事

**As a** 开发者,  
**I want** 实现与目标MCU的波特率协商,  
**so that** 后续数据传输可使用高速波特率。

### 验收标准

1. ✅ 实现 `stc_negotiate_baudrate()` 函数
2. ✅ 根据目标 MCU 系列选择合适的协商流程
3. ✅ 支持协商至 230400/115200/57600/38400 等常用波特率（最高 230400bps）
4. ✅ 协商成功后自动切换本机波特率
5. ✅ 协商失败时回退到安全波特率
6. ✅ 记录实际协商成功的波特率供后续使用

### 技术说明

- 波特率协商流程：
  1. 握手成功后，根据 MCU 主频计算支持的波特率列表
  2. 发送波特率协商命令，指定目标波特率
  3. MCU 返回确认后，切换本机波特率
  4. 发送测试数据验证通信正常
- STC89 系列不支持波特率协商，固定使用握手波特率
- STC15/8 系列支持高达 230400bps 的波特率
- 协商失败时应保持原波特率

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 波特率协商配置 */
typedef struct {
    uint32_t target_baudrate;   /* 目标波特率 */
    uint32_t mcu_freq;          /* MCU 主频 */
} stc_baudrate_config_t;

/* 波特率协商结果 */
typedef struct {
    bool     success;
    uint32_t actual_baudrate;   /* 实际协商成功的波特率 */
} stc_baudrate_result_t;

int stc_negotiate_baudrate(serial_dev_t *serial, const stc_baudrate_config_t *config, stc_baudrate_result_t *result);
```

### 完成定义（DoD）

- [ ] 波特率协商函数实现完成
- [ ] 成功协商至 115200bps 或更高
- [ ] 协商失败后能回退到安全波特率
- [ ] 高速传输稳定，无数据丢失
- [ ] 代码通过代码规范检查

---

## 本史诗完成标准

- [ ] 所有 4 个故事完成
- [ ] 握手和识别功能在多款 STC 芯片上测试通过
- [ ] 波特率协商稳定
- [ ] 为 Epic 3 的擦除和编程功能做好准备

