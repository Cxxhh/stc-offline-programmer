# Epic 5: 高级功能与优化 - 用户故事

## 史诗目标

实现EEPROM烧录、RC振荡器校准、设备选项配置等高级功能，优化系统性能和稳定性，完善批量烧录模式。

## 关联需求

| 需求ID | 描述 | 优先级 |
|--------|------|--------|
| FR8 | 系统应支持将数据烧录到STC单片机的IAP/EEPROM区域 | P1 |
| FR13 | 系统应支持STC15/8系列的内部RC振荡器频率校准功能 | P2 |
| FR14 | 系统应支持配置目标MCU的熔丝位/选项字节 | P2 |
| FR16 | 系统应支持批量烧录模式，实现连续自动烧录 | P3 |

---

## Story S5.1: EEPROM烧录功能

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S5.1 |
| **优先级** | 🟡 P2 |
| **估算点数** | 5 |
| **前置依赖** | S3.4 |

### 用户故事

**As a** 用户,  
**I want** 烧录数据到STC单片机的EEPROM区,  
**so that** 我能预置应用需要的初始数据。

### 验收标准

1. ✅ 实现 `stc_program_eeprom()` 函数
2. ✅ 支持指定 EEPROM 起始地址
3. ✅ 支持从独立的数据文件读取 EEPROM 数据
4. ✅ EEPROM 烧录与 Code Flash 烧录可独立执行
5. ✅ 烧录后进行数据校验
6. ✅ UI 界面支持选择是否烧录 EEPROM

### 技术说明

- EEPROM 地址空间与 Code Flash 分离
- STC 各系列 EEPROM 容量不同：
  - STC89 系列：无 EEPROM
  - STC12/15/8 系列：512B ~ 64KB
- EEPROM 数据文件格式：`.eep`（二进制）或 `.hex`
- 烧录流程：
  1. 检查目标 MCU 是否支持 EEPROM
  2. 读取 EEPROM 数据文件
  3. 发送 EEPROM 编程命令
  4. 校验写入数据

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* EEPROM 编程配置 */
typedef struct {
    uint32_t start_addr;        /* EEPROM 起始地址 */
    uint32_t timeout_ms;
    stc_progress_callback_t progress_cb;
} stc_eeprom_config_t;

int stc_program_eeprom(serial_dev_t *serial, const uint8_t *data, uint32_t size, const stc_eeprom_config_t *config, stc_program_result_t *result);
int stc_verify_eeprom(serial_dev_t *serial, const uint8_t *data, uint32_t size, const stc_verify_config_t *config, stc_verify_result_t *result);
```

### 完成定义（DoD）

- [ ] EEPROM 烧录函数实现完成
- [ ] 成功烧录并读回验证
- [ ] UI 支持 EEPROM 烧录选项
- [ ] 不支持 EEPROM 的 MCU 给出提示
- [ ] 代码通过代码规范检查

---

## Story S5.2: RC振荡器校准功能

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S5.2 |
| **优先级** | 🟡 P2 |
| **估算点数** | 5 |
| **前置依赖** | S3.4 |

### 用户故事

**As a** 用户,  
**I want** 校准STC15/8系列的内部RC振荡器,  
**so that** 目标MCU能以更精确的频率运行。

### 验收标准

1. ✅ 实现 `stc_calibrate_rc()` 函数
2. ✅ 支持指定目标频率（如 11.0592MHz、22.1184MHz）
3. ✅ 正确发送 RC 校准协议命令
4. ✅ 获取并显示校准结果
5. ✅ 仅对支持校准的 MCU 型号启用此功能
6. ✅ UI 界面支持校准参数配置

### 技术说明

- RC 校准原理：
  - STC15/8 系列内置 RC 振荡器，出厂有一定误差
  - 通过 ISP 协议可以微调 RC 频率
  - 校准值写入 MCU 内部寄存器，掉电保持
- 支持的频率：
  - 5.5296MHz、11.0592MHz、22.1184MHz、33.1776MHz
  - 自定义频率（范围限制）
- 校准流程：
  1. 发送校准命令，指定目标频率
  2. MCU 内部调整 RC 参数
  3. 返回校准结果（实际频率、误差率）

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 预设频率枚举 */
typedef enum {
    RC_FREQ_5_5296MHZ = 0,
    RC_FREQ_11_0592MHZ,
    RC_FREQ_22_1184MHZ,
    RC_FREQ_33_1776MHZ,
    RC_FREQ_CUSTOM
} rc_freq_preset_t;

/* RC 校准配置 */
typedef struct {
    rc_freq_preset_t preset;
    uint32_t custom_freq;       /* 自定义频率 (Hz) */
} stc_rc_config_t;

/* RC 校准结果 */
typedef struct {
    bool     success;
    uint32_t actual_freq;       /* 实际校准频率 (Hz) */
    float    error_rate;        /* 误差率 (%) */
} stc_rc_result_t;

int stc_calibrate_rc(serial_dev_t *serial, const stc_rc_config_t *config, stc_rc_result_t *result);
bool stc_is_rc_calibration_supported(const stc_mcu_model_t *model);
```

### 完成定义（DoD）

- [ ] RC 校准函数实现完成
- [ ] 成功校准 STC15/8 系列芯片
- [ ] 校准结果与预期接近（误差 < 1%）
- [ ] 不支持校准的 MCU 给出提示
- [ ] 代码通过代码规范检查

---

## Story S5.3: 批量烧录模式

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S5.3 |
| **优先级** | 🟢 P3 |
| **估算点数** | 8 |
| **前置依赖** | S4.5 |

### 用户故事

**As a** 生产线操作员,  
**I want** 使用批量烧录模式,  
**so that** 我能快速连续烧录多个芯片。

### 验收标准

1. ✅ 实现批量烧录模式状态机
2. ✅ 一次烧录完成后自动等待下一个目标
3. ✅ 检测到新目标自动开始烧录
4. ✅ 统计并显示烧录成功/失败数量
5. ✅ 支持按键退出批量模式
6. ✅ 失败时蜂鸣器或 LED 提示

### 技术说明

- 批量模式流程：
  ```
  进入批量模式 → 等待目标 → 检测到目标 → 自动烧录 → 显示结果 → 等待目标...
       ↑                                                        │
       └────────────────────[取消键]──────────────────────────────┘
  ```
- 自动检测机制：
  - 周期性发送握手信号（间隔 500ms）
  - 检测到响应后自动开始烧录
  - 烧录完成后继续检测
- 统计信息：
  - 总烧录数量
  - 成功数量
  - 失败数量
  - 平均烧录时间
- 声光提示：
  - 成功：绿色 LED + 短促蜂鸣
  - 失败：红色 LED + 长蜂鸣

### 接口定义

```c
/* APP/app_isp.h */

/* 批量模式统计 */
typedef struct {
    uint32_t total_count;       /* 总烧录次数 */
    uint32_t success_count;     /* 成功次数 */
    uint32_t fail_count;        /* 失败次数 */
    uint32_t avg_time_ms;       /* 平均烧录时间 */
} batch_statistics_t;

void app_isp_enter_batch_mode(void);
void app_isp_exit_batch_mode(void);
bool app_isp_is_batch_mode(void);
void app_isp_get_batch_statistics(batch_statistics_t *stats);
```

### 完成定义（DoD）

- [ ] 批量模式实现完成
- [ ] 连续烧录 10+ 芯片测试通过
- [ ] 统计信息准确
- [ ] 声光提示正常
- [ ] 代码通过代码规范检查

---

## Story S5.4: 设备选项配置

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S5.4 |
| **优先级** | 🟡 P2 |
| **估算点数** | 5 |
| **前置依赖** | S3.4 |

### 用户故事

**As a** 高级用户,  
**I want** 配置目标MCU的熔丝位和选项字节,  
**so that** 我能定制MCU的启动和运行参数。

### 验收标准

1. ✅ 实现设备选项读取功能
2. ✅ 实现设备选项写入功能
3. ✅ 支持常用选项的图形化配置界面
4. ✅ 对危险选项（如 ISP 禁用）给出警告
5. ✅ 配置前显示当前值和新值对比
6. ✅ 支持从配置文件加载预设选项

### 技术说明

- STC MCU 常用选项：
  - 看门狗使能/禁用
  - 低压复位阈值
  - 内部 RC 频率选择
  - P3.0/P3.1 复用功能
  - ISP 下载使能（危险选项）
  - 上电延时时间
- 选项字节格式因 MCU 系列而异
- 危险选项警告：
  - 禁用 ISP 后将无法再次编程
  - 需要二次确认

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 设备选项结构（通用） */
typedef struct {
    bool     wdt_enable;        /* 看门狗使能 */
    uint8_t  lvr_threshold;     /* 低压复位阈值 */
    uint32_t rc_freq;           /* RC 频率 */
    bool     isp_enable;        /* ISP 使能（危险） */
    uint16_t powerup_delay;     /* 上电延时 (ms) */
    uint8_t  raw_data[16];      /* 原始选项数据 */
} stc_device_options_t;

int stc_read_options(serial_dev_t *serial, stc_device_options_t *options);
int stc_write_options(serial_dev_t *serial, const stc_device_options_t *options);
bool stc_is_dangerous_option(const stc_device_options_t *options);
```

### 完成定义（DoD）

- [ ] 选项读写函数实现完成
- [ ] 能正确读取当前选项
- [ ] 能成功修改选项
- [ ] 危险选项警告正常
- [ ] UI 配置界面可用
- [ ] 代码通过代码规范检查

---

## 本史诗完成标准

- [ ] 所有 4 个故事完成
- [ ] 高级功能测试通过
- [ ] 系统稳定性经过长时间测试
- [ ] **项目完整交付！**

---

## 未来扩展（P4 - 长期规划）

以下功能可考虑在后续版本中实现：

| 功能 | 描述 |
|------|------|
| STC32 支持 | 支持 STC32 系列（MCS-251 架构）芯片烧录 |
| USB 接口 | 支持 USB 连接，兼容 PC 端软件 |
| OTA 升级 | 支持烧录器自身固件的在线升级 |
| 多语言 UI | 支持中/英文界面切换 |
| 烧录日志 | SD 卡存储烧录历史记录 |
| 加密固件 | 支持加密固件的烧录 |

