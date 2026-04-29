# bestarc_bluetoothmodel_BTC_500DP_MAX

## 1. 工程说明

该工程用于实现 PB03F 蓝牙桥接功能：

- 通过 BLE 广播上报设备状态
- 通过 BLE GATT FFE0/FFE1 与 App 通讯
- 通过 UART1 与下位机进行 12 字节固定包协议通讯
- 支持通过宏切换两种设备型号：
  - `BTC_500DP_PRO`
  - `BTC_500DP_MAX`

当前工程中：

- `BTC_500DP_PRO`：设备类型 `0x0001`
- `BTC_500DP_MAX`：设备类型 `0x0003`
- `BTC_500DP_MAX` 比旧型号多了 `气刨` 状态，对应 `mode = 0x0003`

---

## 2. 型号切换方法

型号切换在 `main.c` 中完成。

位置：

- `example/ble_peripheral/bestarc_bluetoothmodel_BTC_500DP_MAX/main.c`

相关宏：

```c
#define BTC_500DP_PRO         1
#define BTC_500DP_MAX         2
#define BTC_500DP_MODEL       BTC_500DP_MAX
//#define BTC_500DP_MODEL       BTC_500DP_PRO
```

### 使用方法

编译 `MAX`：

```c
#define BTC_500DP_MODEL       BTC_500DP_MAX
```

编译 `PRO`：

```c
#define BTC_500DP_MODEL       BTC_500DP_PRO
```

切换后会自动影响以下内容：

- 广播中的设备类型字段
- Device Information Service 的型号字符串
- App 识别到的设备型号信息

---

## 3. 协议说明

### UART 参数

- 波特率：`19200`
- 数据位：`8`
- 校验位：`N`
- 停止位：`1`

### UART 数据包格式

每包固定 `12` 字节：

- 包头 `2` 字节
- 设备类型 `2` 字节
- 命令码 `2` 字节
- 数据区 `2` 字节
- 校验码 `2` 字节
- 包尾 `2` 字节

当前代码中的协议字节定义在：

- `source/bys_uart.h`

当前工程使用：

- 包头：`AA 55`
- 包尾：`BB 55`

### 模式字段

`mode` 字段定义：

- `0x0000`：钢板
- `0x0001`：网格
- `0x0002`：除锈
- `0x0003`：气刨（仅 MAX 使用）

---

## 4. BLE 功能说明

### 广播内容

广播数据在以下文件中维护：

- `source/bys_bridge.c`
- `source/bys_bridge.h`

广播 Manufacturer Data 中包含：

- MAC 地址
- 设备类型
- 模式
- 2T/4T
- 电流
- 后气时间
- 维弧时间
- 气压单位

### GATT 服务

GATT 服务实现文件：

- `source/sbpProfile_ota.h`
- `source/sbpProfile_ota.c`

服务定义：

- Service UUID：`0xFFE0`
- Characteristic UUID：`0xFFE1`

功能：

- App 写入 `FFE1`：下发 12 字节控制包到下位机
- 固件通知 `FFE1`：把下位机返回的 12 字节原始包透传给 App

---

## 5. 主要功能文件说明

### 5.1 启动与型号选择

- `main.c`

功能：

- 芯片基础初始化
- BLE 内存初始化
- RF 初始化
- 看门狗初始化
- 型号宏切换
- 导出当前型号信息：
  - `g_bys_model_dev_type`
  - `g_bys_model_name`
  - `g_bys_model_name_len`

### 5.2 OSAL 任务入口

- `source/OSAL_bys_bridge.c`

功能：

- 注册 BLE 协议栈任务
- 注册应用任务 `BYS_Bridge_ProcessEvent`
- 在 OSAL 初始化阶段调用 `BYS_Bridge_Init`

### 5.3 应用主入口

- `source/bys_bridge_Main.c`

功能：

- 实现 `app_main()`
- 启动 OSAL 系统主循环

### 5.4 BLE 桥接主逻辑

- `source/bys_bridge.c`
- `source/bys_bridge.h`

功能：

- 配置广播参数
- 填充广播数据
- 读取本机 MAC 并写入广播
- 根据型号写入设备类型
- 更新广播状态字段
- 处理 BLE 连接/断连事件
- 接收 App 写入数据并转发给 UART
- 接收 UART 数据并通过 Notify 上报给 App

### 5.5 UART 协议与状态同步

- `source/bys_uart.c`
- `source/bys_uart.h`

功能：

- UART1 初始化
- 轮询下位机查询命令
- 接收并解析 12 字节协议包
- 校验包头、包尾、校验和
- 更新全局状态 `g_bys_state`
- 维护发送队列
- 透传下位机响应到 BLE 上层

### 5.6 GATT 服务实现

- `source/sbpProfile_ota.c`
- `source/sbpProfile_ota.h`

功能：

- 创建 FFE0/FFE1 服务
- 处理 App 对 FFE1 的读写
- 处理 Notify 开关
- 提供 `simpleProfile_Notify()` 给桥接层上报数据

---

## 6. 数据流说明

### App -> BLE -> UART -> 下位机

流程：

1. App 写入 `FFE1`
2. `sbpProfile_ota.c` 触发写回调
3. `bys_bridge.c` 中 `simpleProfileChangeCB()` 读取数据
4. 调用 `bys_uart_send_app_cmd()` 入队发送
5. `bys_uart.c` 通过 UART1 发给下位机

### 下位机 -> UART -> BLE -> App

流程：

1. 下位机通过 UART1 返回 12 字节数据
2. `bys_uart.c` 在中断回调中缓存数据
3. `bys_uart_process_rx()` 完成包解析和校验
4. 更新 `g_bys_state`
5. `bys_bridge.c` 刷新广播字段
6. `bys_bridge.c` 调用 `simpleProfile_Notify()` 通过 `FFE1` 通知 App

### 下位机 -> 状态 -> 广播

流程：

1. `bys_uart.c` 轮询查询模式、电流、后气等状态
2. 下位机返回查询响应
3. `g_bys_state` 被更新
4. `bys_bridge.c` 将状态写回广播数据

---

## 7. 当前与型号切换直接相关的文件

如果只需要切换型号或维护型号功能，重点关注：

- `main.c`
- `source/bys_bridge.c`
- `source/bys_uart.h`

说明：

- `main.c`：切换型号宏
- `bys_bridge.c`：广播设备类型、设备型号字符串
- `bys_uart.h`：协议状态字段定义，例如 `mode`

---

## 8. 修改建议

### 新增型号时

至少同步修改以下内容：

- `main.c` 中新增型号宏
- `main.c` 中新增设备类型值
- `main.c` 中新增型号字符串
- `source/bys_uart.h` 中补充状态字段取值说明
- `source/bys_bridge.c` 中确认广播字段是否需要扩展

### 修改协议时

重点检查：

- `source/bys_uart.h` 的协议常量
- `source/bys_uart.c` 的打包与解包逻辑
- `source/bys_bridge.c` 的广播字段映射
- `source/sbpProfile_ota.c` 的 BLE 透传长度是否仍为 12 字节
