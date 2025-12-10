# Epic 4: 用户界面与固件管理 - 用户故事

## 史诗目标

实现LCD显示界面和按键交互，集成SD卡FatFs文件系统，提供固件选择和管理功能，形成完整的用户操作体验。

## 关联需求

| 需求ID | 描述 | 优先级 |
|--------|------|--------|
| FR9 | 系统应通过LCD显示目标MCU的型号、Flash大小、UID等设备信息 | P1 |
| FR10 | 系统应支持从SD卡读取HEX/BIN固件文件进行烧录 | P1 |
| FR11 | 系统应支持在SD卡上存储和管理多个固件文件 | P1 |
| FR12 | 系统应提供按键操作界面，支持固件选择、烧录启动等基本操作 | P1 |
| NFR6 | 系统应在烧录失败时提供明确的错误提示 | P1 |

---

## Story S4.1: LCD显示封装层实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S4.1 |
| **优先级** | 🟠 P1 |
| **估算点数** | 3 |
| **前置依赖** | S1.1 |

### 用户故事

**As a** 开发者,  
**I want** 基于现有LCD驱动（`Src/lcd.c` + `Inc/lcd.h`）实现显示封装层,  
**so that** 能够统一管理屏幕显示，并确保显示内容与串口输出保持一致。

### 背景说明

- 项目中已存在完整可用的 LCD 驱动（CT117E 竞赛板驱动），无需重新开发
- 本 Story 的重点是封装和显示逻辑，而非底层驱动实现

### 验收标准

1. ✅ 创建 `bsp_display.c/h`，封装现有 `lcd.c` 接口
2. ✅ 实现统一的显示输出接口（同时支持 LCD 和串口输出）
3. ✅ 实现状态栏、信息区、进度区等 UI 布局函数
4. ✅ 实现屏幕清除和区域刷新的高层封装
5. ✅ 验证现有 LCD 驱动调用正常，显示测试图案正确
6. ✅ 确保 LCD 显示内容与串口调试输出保持同步一致

### 技术说明

- 架构定位：
  ```
  APP Layer (app_ui.c)
       │ 调用 disp_service_xxx()
       ▼
  Service Layer (disp_service.c)         ← 高级显示服务
       │ 调用 display_dev_t 接口
       ▼
  BSP Layer (bsp_display.c)              ← 封装层
       │ 内部调用 LCD_xxx()
       ▼
  现有驱动 (Src/lcd.c)                    ← L0 底层驱动，不修改
  ```
- LCD 规格：240×320 RGB565（CT117E 竞赛板）
- UI 布局分区：
  - 状态栏（顶部）：显示设备状态、时间
  - 信息区（中部）：显示 MCU 信息、文件信息
  - 进度区（底部）：显示烧录进度、操作提示

### 接口定义

```c
/* BSP/Display/bsp_display.h */

/* 获取显示设备实例 */
display_dev_t *bsp_display_get_device(void);

/* Service/display/disp_service.h */

void disp_service_init(void);
void disp_service_clear(void);
void disp_service_show_status(const char *status);
void disp_service_show_info(uint8_t line, const char *text);
void disp_service_show_progress(uint8_t percent);
void disp_service_show_message(const char *title, const char *message);
```

### 完成定义（DoD）

- [ ] 显示封装层实现完成
- [ ] LCD 显示测试图案正确
- [ ] 串口同步输出正常
- [ ] UI 分区布局清晰
- [ ] 代码通过代码规范检查

---

## Story S4.2: 按键输入驱动实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S4.2 |
| **优先级** | 🟠 P1 |
| **估算点数** | 3 |
| **前置依赖** | S1.1 |

### 用户故事

**As a** 开发者,  
**I want** 实现按键输入的BSP驱动,  
**so that** 用户可以通过按键操作设备。

### 验收标准

1. ✅ 创建 `bsp_key.c/h`，实现按键 GPIO 初始化
2. ✅ 实现按键扫描函数，支持消抖处理
3. ✅ 支持短按、长按事件检测
4. ✅ 实现按键事件回调机制
5. ✅ 支持 4-6 个独立按键（上/下/确认/取消等）
6. ✅ 按键响应延迟小于 50ms

### 技术说明

- 按键定义（参考 CT117E 竞赛板）：
  - KEY_UP: 向上选择
  - KEY_DOWN: 向下选择
  - KEY_OK: 确认/进入
  - KEY_CANCEL: 取消/返回
- 消抖时间：20ms
- 长按阈值：1000ms
- 使用定时器中断进行周期扫描（10ms 间隔）

### 接口定义

```c
/* BSP/Key/bsp_key.h */

/* 按键定义 */
typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_OK,
    KEY_CANCEL
} key_id_t;

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_RELEASE
} key_event_t;

/* 按键事件结构 */
typedef struct {
    key_id_t    key;
    key_event_t event;
} key_event_data_t;

/* 按键事件回调 */
typedef void (*key_callback_t)(const key_event_data_t *event);

void bsp_key_init(void);
void bsp_key_scan(void);  /* 在定时器中断中调用 */
void bsp_key_set_callback(key_callback_t callback);
key_event_data_t bsp_key_get_event(void);
```

### 完成定义（DoD）

- [ ] 按键驱动实现完成
- [ ] 所有按键响应正常
- [ ] 消抖工作正常，无误触发
- [ ] 长按检测准确
- [ ] 代码通过代码规范检查

---

## Story S4.3: SD卡与FatFs集成

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S4.3 |
| **优先级** | 🟠 P1 |
| **估算点数** | 5 → 2（BSP层已完成） |
| **前置依赖** | S1.1 |

### 用户故事

**As a** 开发者,  
**I want** 集成SD卡驱动和FatFs文件系统,  
**so that** 能够读取存储在SD卡上的固件文件。

### 现有基础设施 ✅

> **重要**：SD卡驱动和FatFs已完整实现并验证通过，无需重新开发。

| 组件 | 文件 | 状态 |
|------|------|------|
| BSP公共定义 | `BSP/bsp_common.h` | ✅ 已完成 |
| SPI DMA管理器 | `BSP/bsp_spi.c/h` | ✅ 已完成 |
| SD卡驱动 | `BSP/bsp_sdcard.c/h` | ✅ 已完成 |
| FatFs核心 | `FatFS/src/ff.c/h` | ✅ 已完成 |
| 磁盘IO适配 | `FatFS/src/diskio_sdcard.c` | ✅ 已完成 |
| DMA中断处理 | `Src/stm32g4xx_it.c` | ✅ 已完成 |

**已验证功能**：SD卡初始化、扇区读写、FatFs挂载、文件读写

### 验收标准

1. ✅ 创建 `bsp_sdcard.c/h`，实现 SPI 模式 SD 卡驱动 **（已完成）**
2. ✅ 集成 FatFs 中间件 **（已完成）**
3. ✅ 实现 SD 卡检测和初始化 **（已完成）**
4. ⏳ 实现文件列表读取功能（file_manager服务层待实现）
5. ⏳ 实现文件打开、读取、关闭操作（file_manager服务层待实现）
6. ⏳ 能够正确读取 HEX/BIN 格式固件文件

### 技术说明

- SD 卡通信：SPI1 接口 (PA5/PA6/PA7)，CS引脚 PB10
- DMA通道：DMA1_Channel1 (RX)，DMA1_Channel2 (TX)
- FatFs 配置：
  - 文件系统：FAT32
  - 代码页：简体中文（936）或 ASCII
  - 长文件名支持：启用
- 固件文件格式支持：
  - `.bin`：二进制格式，直接读取
  - `.hex`：Intel HEX 格式，需要解析
- 文件存放目录：SD卡根目录或 `/firmware/` 目录

### 接口定义

#### BSP层接口（已实现）

```c
/* BSP/bsp_sdcard.h */
bsp_sdcard_result_t bsp_sdcard_init(void);
bool bsp_sdcard_is_inserted(void);
bsp_sdcard_result_t bsp_sdcard_read_sector(uint32_t sector, uint8_t* buf);
bsp_sdcard_result_t bsp_sdcard_write_sector(uint32_t sector, const uint8_t* buf);
```

#### Service层接口（待实现）

```c
/* Service/file_mgr/file_manager.h */

/* 文件信息结构 */
typedef struct {
    char     name[64];
    uint32_t size;
    bool     is_hex;
} firmware_file_info_t;

int file_mgr_init(void);
int file_mgr_get_file_list(firmware_file_info_t *list, uint16_t max_count, uint16_t *actual_count);
int file_mgr_read_firmware(const char *filename, uint8_t *buffer, uint32_t max_size, uint32_t *actual_size);
int file_mgr_parse_hex(const char *filename, uint8_t *buffer, uint32_t max_size, uint32_t *actual_size);
```

### 完成定义（DoD）

- [x] SD 卡驱动实现完成
- [x] FatFs 挂载成功
- [ ] file_manager 服务层实现
- [ ] 能够列出 SD 卡中的固件文件
- [ ] BIN/HEX 文件读取正确
- [ ] 代码通过代码规范检查

### 参考文档

- 详细设计见：[2.6-SD卡驱动集成设计.md](../architecture/2.6-SD卡驱动集成设计.md)
- 集成说明：[README_BSP_INTEGRATION.md](../../BSP/README_BSP_INTEGRATION.md)

---

## Story S4.4: 主界面显示实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S4.4 |
| **优先级** | 🟠 P1 |
| **估算点数** | 5 |
| **前置依赖** | S4.1 |

### 用户故事

**As a** 用户,  
**I want** 在主界面看到设备状态和关键信息,  
**so that** 我能了解烧录器的当前状态。

### 验收标准

1. ✅ 创建 `app_ui.c/h`，实现主界面布局
2. ✅ 显示当前烧录器状态（空闲/烧录中/完成/错误）
3. ✅ 显示当前选择的固件文件名
4. ✅ 显示目标 MCU 信息（型号、Flash 大小）
5. ✅ 显示最近操作结果
6. ✅ 界面刷新无明显闪烁

### 技术说明

- 主界面布局设计：
  ```
  ┌────────────────────────────────────┐
  │  STC 离线烧录器 v1.0    [空闲]    │  ← 标题栏
  ├────────────────────────────────────┤
  │  固件: test_led.bin (2.5KB)       │  ← 固件信息
  │  目标: STC15W4K32S4               │  ← MCU 信息
  │  Flash: 32KB  EEPROM: 4KB         │
  │  UID: 12-34-56-78-9A-BC-DE        │
  ├────────────────────────────────────┤
  │  [↑] 选择固件  [OK] 开始烧录      │  ← 操作提示
  │  [↓] 系统设置  [×] 取消操作       │
  ├────────────────────────────────────┤
  │  最近: 烧录成功 (2.3s)            │  ← 状态信息
  └────────────────────────────────────┘
  ```
- 界面刷新策略：
  - 只刷新变化的区域，避免全屏刷新
  - 状态变化时立即刷新
  - 进度条使用增量更新

### 接口定义

```c
/* APP/app_ui.h */

/* UI 状态枚举 */
typedef enum {
    UI_STATE_MAIN,          /* 主界面 */
    UI_STATE_FILE_SELECT,   /* 文件选择 */
    UI_STATE_PROGRAMMING,   /* 烧录中 */
    UI_STATE_RESULT,        /* 结果显示 */
    UI_STATE_SETTINGS       /* 系统设置 */
} ui_state_t;

void app_ui_init(void);
void app_ui_update(void);
void app_ui_set_state(ui_state_t state);
void app_ui_show_mcu_info(const stc_mcu_info_t *info);
void app_ui_show_firmware_info(const firmware_file_info_t *info);
void app_ui_show_progress(uint8_t percent, const char *status);
void app_ui_show_result(bool success, const char *message);
```

### 完成定义（DoD）

- [ ] 主界面布局实现完成
- [ ] 所有信息正确显示
- [ ] 界面刷新流畅
- [ ] 状态切换响应迅速
- [ ] 代码通过代码规范检查

---

## Story S4.5: 固件选择与烧录流程

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S4.5 |
| **优先级** | 🟠 P1 |
| **估算点数** | 5 |
| **前置依赖** | S3.4, S4.2, S4.3, S4.4 |

### 用户故事

**As a** 用户,  
**I want** 通过按键选择固件并启动烧录,  
**so that** 我能完成离线烧录操作。

### 验收标准

1. ✅ 实现固件文件列表浏览界面
2. ✅ 支持上下键选择固件文件
3. ✅ 确认键选择固件并返回主界面
4. ✅ 主界面确认键启动烧录流程
5. ✅ 烧录过程中显示实时进度
6. ✅ 烧录完成后显示结果（成功/失败及原因）

### 技术说明

- 用户操作流程：
  ```
  主界面 ─[↑]─▶ 文件选择界面 ─[↑/↓]─▶ 选择文件 ─[OK]─▶ 返回主界面
     │                                                    │
     └────────────────[OK]─────────────────────────────────▶ 开始烧录
                                                           │
     ┌─────────────────────────────────────────────────────┘
     ▼
  烧录进度界面 ──▶ 结果界面 ─[任意键]─▶ 主界面
  ```
- 文件选择界面设计：
  ```
  ┌────────────────────────────────────┐
  │  选择固件文件 (共 5 个)            │
  ├────────────────────────────────────┤
  │    test_led.bin (2.5KB)           │
  │  ▶ app_main.hex (15KB)            │  ← 当前选中
  │    bootloader.bin (4KB)           │
  │    ...                            │
  ├────────────────────────────────────┤
  │  [↑↓] 选择  [OK] 确认  [×] 返回   │
  └────────────────────────────────────┘
  ```
- 烧录进度界面设计：
  ```
  ┌────────────────────────────────────┐
  │  正在烧录...                       │
  ├────────────────────────────────────┤
  │  ████████████░░░░░░░░  60%        │  ← 进度条
  │  已写入: 9.6KB / 16KB             │
  │  耗时: 5.2s                       │
  ├────────────────────────────────────┤
  │  [×] 取消烧录                     │
  └────────────────────────────────────┘
  ```

### 接口定义

```c
/* APP/app_isp.h */

/* 烧录应用接口 */
void app_isp_init(void);
void app_isp_process(void);  /* 主循环调用 */
void app_isp_start(const char *filename);
void app_isp_abort(void);

/* 烧录事件处理（ISP 回调） */
void app_isp_on_event(const isp_event_data_t *event);
```

### 完成定义（DoD）

- [ ] 完整烧录流程实现完成
- [ ] 固件选择功能正常
- [ ] 烧录进度显示准确
- [ ] 结果显示清晰（成功/失败原因）
- [ ] 用户可以取消烧录操作
- [ ] 代码通过代码规范检查

---

## 本史诗完成标准

- [ ] 所有 5 个故事完成
- [ ] UI 界面美观、响应流畅
- [ ] SD 卡固件管理功能正常
- [ ] 完整的用户操作闭环
- [ ] **里程碑达成：完整产品 V1.0！**

