# bestarc_bluetoothmodel_BTC_500DP_PRO

## 1. 工程说明

基于 PHY62XX SDK 3.1.5 的 BLE 蓝牙桥接固件，适用于 **BTC500DP PRO** 焊机。

核心功能：

- 通过 UART1 与下位机进行 12 字节固定包协议通讯，轮询查询设备状态
- 通过 BLE 广播上报设备状态（模式、电流、后气、维弧等）
- 通过 BLE GATT（FFE0/FFE1）实现 App 与下位机的双向透传

设备型号：`BTC500DP PRO`，广播设备类型 `0x0001`

---

## 2. 协议说明

### UART 参数

- 波特率：`19200`，8N0
- 端口：UART1（TX=P24, RX=P23）

### 数据包格式

每包固定 12 字节：

| 字段 | 包头 | 设备类型 | 命令码 | 数据区 | 校验码 | 包尾 |
|------|------|---------|-------|-------|-------|------|
| 长度 | 2B   | 2B      | 2B    | 2B    | 2B    | 2B   |

- 包头：`0xAA 0x55`
- 包尾：`0xBB 0x55`
- 校验码 = 命令码 + 数据区

### 上位机发送时设备类型字段

- `0x8000`：APP 已连接
- `0x0000`：APP 未连接

### 查询命令（8 条循环轮询）

| 命令码 | 说明 | 响应码 |
|--------|------|--------|
| `0x0002` | 模式（钢板/网格/除锈） | `0x0082` |
| `0x0003` | 2T/4T | `0x0083` |
| `0x0004` | 电流 | `0x0084` |
| `0x0005` | 后气时间 | `0x0085` |
| `0x0006` | 维弧时间 | `0x0086` |
| `0x0007` | 气压单位 | `0x0087` |
| `0x0008` | 报警状态 | `0x0088` |
| `0x0009` | 输入电压 | `0x0089` |

轮询间隔 250ms/包，8 包一轮 = 2 秒。

### 模式字段

- `0x0000`：钢板
- `0x0001`：网格
- `0x0002`：除锈

---

## 3. BLE 广播

广播数据 31 字节，包含 3 个 AD Structure：

- **AD1**（3B）：Flags `02 01 06`
- **AD2**（5B）：Complete Local Name `"BYS"`
- **AD3**（23B）：Manufacturer Specific Data

AD3 字段布局：

| 偏移 | 长度 | 字段 |
|------|------|------|
| 10-15 | 6B | MAC 地址（大端序） |
| 16-17 | 2B | 设备类型 `0x0001` |
| 18-19 | 2B | 模式 |
| 20-21 | 2B | 2T/4T |
| 22-23 | 2B | 电流 |
| 24-25 | 2B | 后气时间 |
| 26-27 | 2B | 维弧时间 |
| 28-29 | 2B | 气压单位 |

### GATT 服务

- Service UUID：`0xFFE0`
- Characteristic UUID：`0xFFE1`（Write + Notify，12 字节）
- App 写入 FFE1 → 转发给下位机
- 下位机响应 → Notify 给 App

---

## 4. 文件说明

### 启动入口

- **`main.c`**：芯片初始化、BLE 内存配置、RF 初始化、看门狗、调用 `app_main()`

### OSAL 任务注册

- **`source/OSAL_bys_bridge.c`**：注册 BLE 协议栈任务和应用任务 `BYS_Bridge_ProcessEvent`
- **`source/bys_bridge_Main.c`**：实现 `app_main()`，启动 OSAL 主循环

### BLE 桥接主逻辑

- **`source/bys_bridge.h`**：OSAL 事件位定义、广播数据偏移定义、轮询间隔配置
- **`source/bys_bridge.c`**：
  - 广播数据构造与更新
  - GAP 参数配置
  - BLE 连接/断连处理
  - App 写入回调 → 转发给 UART
  - UART 回调 → Notify 给 App
  - 定时器驱动轮询

### UART 协议

- **`source/bys_uart.h`**：协议常量（包头包尾、命令码、响应码）、设备状态结构体
- **`source/bys_uart.c`**：
  - UART1 初始化
  - 12 字节协议包的打包与解析
  - 发送队列管理（3 包深度）
  - 8 条查询命令循环轮询
  - 接收校验与状态更新（`g_bys_state`）
  - App 控制指令入队与发送

### GATT Profile

- **`source/sbpProfile_ota.h`**：FFE0/FFE1 UUID 定义、接口声明
- **`source/sbpProfile_ota.c`**：FFE0 服务注册、FFE1 读写处理、Notify 实现

### 编译相关

- **`bys_bridge.uvprojx`**：Keil MDK 工程文件
- **`scatter_load.sct`**：链接脚本

---

## 5. 数据流

### App → 下位机

1. App 写入 FFE1（12 字节）
2. `simpleProfileChangeCB()` 读取数据
3. `bys_uart_send_app_cmd()` 修正设备类型字段为 `0x8000` 后入队
4. UART1 发送给下位机

### 下位机 → App

1. 下位机返回 12 字节响应
2. `uart_rx_cb()` 缓存数据，触发 `BYS_UART_RX_EVT`
3. `bys_uart_process_rx()` 校验并解析
4. `apply_response()` 更新 `g_bys_state`
5. `bys_uart_rx_callback()` → `simpleProfile_Notify()` 透传给 App
6. `bys_update_adv_data()` 刷新广播字段

---

## 6. IO 分配

| GPIO | 功能 |
|------|------|
| P02/P03 | SWD 调试 |
| P09/P10 | UART0（DEBUG LOG） |
| P23 | UART1 RX（下位机） |
| P24 | UART1 TX（下位机） |

---

## 7. 关键配置参数

| 参数 | 值 | 位置 |
|------|-----|------|
| BLE 广播间隔 | 100ms | `bys_bridge.c` |
| BLE 连接超时 | 5s | `bys_bridge.c` |
| UART 波特率 | 19200 | `bys_uart.h` |
| 轮询间隔 | 250ms/包 | `bys_bridge.h` |
| TX 队列深度 | 3 包 | `bys_uart.c` |
| RF 发射功率 | 0dBm | `main.c` |
| BLE 包版本 | 5.1 | `main.c` |
