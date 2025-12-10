# Epic 3: 擦除与编程功能 - 用户故事

## 史诗目标

实现STC单片机的擦除和编程核心功能，包括Flash擦除、代码烧录、数据校验，形成最小可用的烧录功能闭环。

## 关联需求

| 需求ID | 描述 | 优先级 |
|--------|------|--------|
| FR3 | 系统应支持将用户代码烧录到STC单片机的Code Flash区域 | P0 |
| FR4 | 系统应支持对STC单片机进行全片擦除操作 | P0 |
| FR5 | 系统应在烧录完成后进行数据完整性校验 | P0 |
| NFR2 | 单次烧录64KB代码的时间应小于30秒 | P0 |
| NFR4 | 烧录成功率应达到99.9%以上 | P0 |

---

## Story S3.1: Flash擦除功能实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S3.1 |
| **优先级** | 🔴 P0 |
| **估算点数** | 3 |
| **前置依赖** | S2.4 |

### 用户故事

**As a** 开发者,  
**I want** 实现Flash擦除命令发送与确认,  
**so that** 能够清除目标MCU的代码区为烧录做准备。

### 验收标准

1. ✅ 实现 `stc_erase_flash()` 函数
2. ✅ 正确构造擦除命令帧并发送
3. ✅ 等待并解析擦除完成响应
4. ✅ 支持全片擦除模式
5. ✅ 实现擦除超时检测（根据 Flash 大小动态调整）
6. ✅ 返回明确的擦除结果状态

### 技术说明

- 擦除命令帧格式：
  ```
  | 帧头 | 方向 | 长度 | CMD(0x84) | 等待时间 | 校验和 | 帧尾 |
  ```
- 擦除时间与 Flash 大小相关：
  - 8KB Flash: ~500ms
  - 64KB Flash: ~2s
  - 128KB Flash: ~4s
- 超时时间 = 预估擦除时间 × 2
- 擦除成功后 MCU 返回确认响应

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 擦除配置 */
typedef struct {
    uint32_t flash_size;        /* Flash 大小 (bytes) */
    uint32_t timeout_ms;        /* 超时时间，0 表示自动计算 */
} stc_erase_config_t;

/* 擦除结果 */
typedef struct {
    bool     success;
    uint32_t elapsed_ms;        /* 实际耗时 */
} stc_erase_result_t;

int stc_erase_flash(serial_dev_t *serial, const stc_erase_config_t *config, stc_erase_result_t *result);
```

### 完成定义（DoD）

- [ ] 擦除函数实现完成
- [ ] 使用实际 STC 芯片测试擦除成功
- [ ] 超时检测工作正常
- [ ] 擦除时间符合预期
- [ ] 代码通过代码规范检查

---

## Story S3.2: 代码烧录功能实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S3.2 |
| **优先级** | 🔴 P0 |
| **估算点数** | 8 |
| **前置依赖** | S3.1 |

### 用户故事

**As a** 开发者,  
**I want** 实现代码数据的分包发送与烧录,  
**so that** 能够将固件写入目标MCU的Flash。

### 验收标准

1. ✅ 实现 `stc_program_flash()` 函数
2. ✅ 支持将固件数据按协议要求分包发送
3. ✅ 每包发送后等待并验证 ACK 响应
4. ✅ 实现发送进度回调，支持进度显示
5. ✅ 支持从指定地址开始烧录
6. ✅ 发送完成后正确结束编程会话

### 技术说明

- 编程流程：
  1. 发送编程起始命令
  2. 分包发送固件数据（每包 128/256 字节）
  3. 等待每包的 ACK 响应
  4. 发送编程结束命令
- 数据包格式：
  ```
  | 帧头 | 方向 | 长度 | CMD(0x80) | 地址(2) | 数据(N) | 校验和 | 帧尾 |
  ```
- 进度回调用于 UI 层显示烧录进度

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 进度回调函数类型 */
typedef void (*stc_progress_callback_t)(uint32_t current, uint32_t total);

/* 编程配置 */
typedef struct {
    uint32_t start_addr;                /* 起始地址，默认 0 */
    uint16_t packet_size;               /* 包大小，默认 128 */
    uint32_t timeout_ms;                /* 单包超时时间 */
    stc_progress_callback_t progress_cb; /* 进度回调 */
} stc_program_config_t;

/* 编程结果 */
typedef struct {
    bool     success;
    uint32_t bytes_written;             /* 实际写入字节数 */
    uint32_t elapsed_ms;                /* 总耗时 */
} stc_program_result_t;

int stc_program_flash(serial_dev_t *serial, const uint8_t *data, uint32_t size, const stc_program_config_t *config, stc_program_result_t *result);
```

### 完成定义（DoD）

- [ ] 编程函数实现完成
- [ ] 成功烧录测试固件到 STC 芯片
- [ ] 进度回调正常触发
- [ ] 64KB 烧录时间 < 30 秒
- [ ] 代码通过代码规范检查

---

## Story S3.3: 数据校验功能实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S3.3 |
| **优先级** | 🔴 P0 |
| **估算点数** | 3 |
| **前置依赖** | S3.2 |

### 用户故事

**As a** 开发者,  
**I want** 实现烧录数据的校验功能,  
**so that** 确保烧录的数据完整正确。

### 验收标准

1. ✅ 实现 `stc_verify_flash()` 函数
2. ✅ 支持计算本地固件校验和
3. ✅ 发送校验命令获取目标 MCU 的校验和
4. ✅ 比对本地与目标校验和
5. ✅ 校验失败时返回详细的错误位置信息
6. ✅ 支持回读校验模式（可选）

### 技术说明

- 校验方式：
  - 方式 1：校验和比对（快速）
  - 方式 2：回读校验（可靠，但较慢）
- 校验和算法：逐字节累加取低 8 位
- 校验命令帧格式：
  ```
  | 帧头 | 方向 | 长度 | CMD(0x83) | 本地校验和 | 校验和 | 帧尾 |
  ```
- 校验失败应返回具体错误信息

### 接口定义

```c
/* Service/isp_core/stc_isp_protocol.h */

/* 校验模式 */
typedef enum {
    VERIFY_MODE_CHECKSUM = 0,   /* 校验和比对 */
    VERIFY_MODE_READBACK        /* 回读校验 */
} stc_verify_mode_t;

/* 校验配置 */
typedef struct {
    stc_verify_mode_t mode;
    uint32_t timeout_ms;
} stc_verify_config_t;

/* 校验结果 */
typedef struct {
    bool     success;
    uint8_t  local_checksum;
    uint8_t  remote_checksum;
    uint32_t mismatch_addr;     /* 校验失败的地址（回读模式） */
} stc_verify_result_t;

int stc_verify_flash(serial_dev_t *serial, const uint8_t *data, uint32_t size, const stc_verify_config_t *config, stc_verify_result_t *result);
```

### 完成定义（DoD）

- [ ] 校验函数实现完成
- [ ] 校验通过的固件运行正常
- [ ] 故意写入错误数据，校验能检出
- [ ] 校验时间合理
- [ ] 代码通过代码规范检查

---

## Story S3.4: ISP状态机实现

### 基本信息

| 属性 | 值 |
|------|-----|
| **ID** | S3.4 |
| **优先级** | 🔴 P0 |
| **估算点数** | 5 |
| **前置依赖** | S3.3 |

### 用户故事

**As a** 开发者,  
**I want** 实现ISP核心状态机,  
**so that** 统一管理烧录流程的各个阶段。

### 验收标准

1. ✅ 创建 `stc_isp_core.c/h`，定义 ISP 状态枚举
2. ✅ 状态包括：空闲、握手中、已连接、擦除中、编程中、校验中、完成、错误
3. ✅ 实现状态转换函数和当前状态查询
4. ✅ 实现完整烧录流程的顺序控制（握手→擦除→编程→校验）
5. ✅ 支持流程中断和错误恢复
6. ✅ 提供烧录完成回调接口

### 技术说明

- 状态机设计参见架构文档 `5-isp核心架构设计.md`
- 状态转换规则：
  ```
  IDLE → HANDSHAKE → CONNECTED → READY → ERASING → ERASED → PROGRAMMING → VERIFYING → COMPLETE
                ↓                           ↓            ↓               ↓
              ERROR ←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←
  ```
- 错误状态可通过 `isp_reset()` 恢复到 IDLE
- 事件回调用于 UI 层更新显示

### 接口定义

```c
/* Service/isp_core/stc_isp_core.h */

/* 参考架构文档中的完整定义 */

/* 核心 API */
isp_error_t isp_init(isp_handle_t *handle, serial_dev_t *serial, ringbuffer_t *rx_buf);
void isp_set_config(isp_handle_t *handle, const isp_config_t *config);
void isp_set_callback(isp_handle_t *handle, isp_event_callback_t callback);

isp_error_t isp_start_handshake(isp_handle_t *handle);
isp_error_t isp_erase(isp_handle_t *handle);
isp_error_t isp_program(isp_handle_t *handle, const uint8_t *data, uint32_t size, uint32_t addr);
isp_error_t isp_verify(isp_handle_t *handle, const uint8_t *data, uint32_t size, uint32_t addr);

/* 一键烧录（集成握手+擦除+编程+校验） */
isp_error_t isp_flash(isp_handle_t *handle, const uint8_t *data, uint32_t size);

isp_state_t isp_get_state(isp_handle_t *handle);
void isp_abort(isp_handle_t *handle);
void isp_reset(isp_handle_t *handle);
```

### 完成定义（DoD）

- [ ] 状态机实现完成
- [ ] 所有状态转换符合设计
- [ ] 一键烧录功能测试通过
- [ ] 错误恢复机制正常
- [ ] 代码通过代码规范检查

---

## 本史诗完成标准

- [ ] 所有 4 个故事完成
- [ ] 完整烧录流程（握手→擦除→编程→校验）测试通过
- [ ] 多款 STC 芯片烧录成功率 > 99%
- [ ] **里程碑达成：MVP 烧录器完成！**
- [ ] 为 Epic 4 的 UI 和固件管理做好准备

