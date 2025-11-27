# STC8 脱机烧录串口流程

以下步骤根据 stcgal 对 STC8 系列（UART 协议）的实现整理，便于在 STM32 等主控上复刻。默认场景为 UART 烧录，BSL 版本 7.2 及以上的补充字段已注明。

## 串口与时序准备
- 串口参数：8 数据位、1 停止位、**偶校验**（STC12/15/8 系列协议要求），握手波特率默认为 2400，编程波特率默认为 115200，可按需要调整。【F:stcgal/protocols.py†L302-L341】【F:doc/USAGE.md†L33-L50】
- 上电/复位后以握手波特率反复发送 `0x7F`（每约 30 ms 一次）脉冲，直到收到以 `46 B9 68` 开头的状态帧（STC BSL 的帧头），再解析其中的频率、BSL 版本和选项信息。【F:stcgal/protocols.py†L326-L336】【F:stcgal/protocols.py†L1993-L2037】

## 频率校准与切换至编程波特率
1. **第一轮校准挑战**：发送 `0x00` 包，长度字段为 12，后续依次给出 12 组粗调 (trim_adj, trim_range) 组合，其中 `trim_adj` 为 0、23、46…、255，`trim_range` 为 0。随后发送 `0xFE` 脉冲等待 MCU 返回 `0x00` 开头的校准计数表。【F:stcgal/protocols.py†L2058-L2073】
2. **选择分频并细化候选**：根据第一轮返回的计数，依次尝试分频因子 1–5，找到能覆盖目标计数的区间，记录对应的 (trim_adj, trim_range)。若全部失败则校准中止。【F:stcgal/protocols.py†L2075-L2082】【F:stcgal/protocols.py†L1551-L1573】
3. **第二轮校准挑战**：围绕选中的 `trim_adj`±1，按 `trim_range` = 0、1、2、3 生成 12 组精调组合再发一次 `0x00` 包，同样在发送后补一个 `0xFE` 脉冲并等待 `0x00` 响应。【F:stcgal/protocols.py†L2085-L2099】
4. **选出最终校准值**：比较第二轮返回的计数与目标计数的误差，选择最优 (trim_adj, trim_range) 作为 `trim_value`，并换算出修正后的内部时钟频率（用于后续下载）。【F:stcgal/protocols.py†L2101-L2105】【F:stcgal/protocols.py†L1575-L1597】
5. **切换编程波特率**：发送 `0x01 0x00 0x00` 起始的切换包，后跟 `(65536 - 24 MHz / (baud*4))` 的 16 位值、`trim_range`、`trim_adj`，以及按 24 MHz 计算的 IAP 等待值。收到 `0x01` 回包后，将本地 UART 切换到目标编程波特率。【F:stcgal/protocols.py†L2107-L2119】

## 编程前握手与锁定检测
- 在成功切到编程波特率后，发送 `0x05` 测试包；若 BSL ≥ 7.2，需追加 `00 00 5A A5` 校验字段。返回单字节 `0x0F` 表示芯片已上锁，`0x05` 表示可以继续。【F:stcgal/protocols.py†L1703-L1723】

## 擦除、写入与收尾
1. **全片/含 EEPROM 擦除**：发送 `0x03`，根据需要跟 `0x00`（仅代码区）或 `0x01`（代码+EEPROM），BSL ≥ 7.2 需补 `00 5A A5`。期望回 `0x03`，并可从响应中取 UID。【F:stcgal/protocols.py†L1727-L1756】
2. **分块写入**：按 128 字节块循环；首块指令 `0x22`，其余块 `0x02`，附上偏移地址和可选的 `5A A5`（BSL ≥ 7.2）以及填充后的数据。每块应回 `0x02 0x54`。【F:stcgal/protocols.py†L1758-L1773】
3. **写入收尾**：若 BSL ≥ 7.2，发送 `0x07 00 00 5A A5` 等待 `0x07 0x54` 完成标记。【F:stcgal/protocols.py†L1775-L1783】
4. **可选：写入选项字节**：使用 `0x04 00 00`（BSL ≥ 7.2 同样附 `5A A5`）+ 40 字节选项表写入，成功返回 `0x04 0x54`。【F:stcgal/protocols.py†L2122-L2135】【F:stcgal/protocols.py†L2141-L2144】

## 断开
- 发送 `0xFF` 复位 BSL，并关闭串口即可完成脱机烧录流程。【F:stcgal/protocols.py†L2137-L2144】