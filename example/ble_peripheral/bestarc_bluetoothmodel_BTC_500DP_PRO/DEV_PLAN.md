# BYS_Bridge 开发计划

## 一、项目概述

PB03F 作为 BLE 透传中继模块：
- **下行**：通过 UART1（19200 8N1）与 BTC500DP PRO 焊机通讯
- **上行**：通过 BLE 与手机 APP 通讯
- **方向**：双向透传

```
[BTC500DP PRO] ←UART1 19200→ [PB03F] ←BLE→ [手机APP]
```

---

## 二、BLE 协议设计

### 广播（Advertising）— 设备状态广播，29字节

| 位置 | 内容 | 字节数 |
|------|------|--------|
| AD1 | `02 01 06` Flags | 3 |
| AD2 | `04 09 'B''Y''S'` 设备名 | 5 |
| AD3 | `16 FF` + MAC(6) + 状态数据(14) | 23 |

AD3 状态数据字段（小端序，各2字节）：

| 偏移(AD3内) | 字段 | 说明 |
|------------|------|------|
| [2\~7]  | MAC  | BLE MAC地址 |
| [8\~9]  | 设备类型 | `0x0001` BTC500DP PRO |
| [10\~11]| 钢板/网格 | 0=钢板,1=网格,2=除锈 |
| [12\~13]| 2T/4T | 0=2T, 1=4T |
| [14\~15]| 电流(A) | 当前电流值 |
| [16\~17]| 后气(s) | 3~15 |
| [18\~19]| 维弧(s) | 3~15 |
| [20\~21]| 气压单位 | 0=PSI,1=MPa,2=BAR |

### GATT（连接后双向透传）

| Characteristic | 方向 | 用途 |
|---------------|------|------|
| CHAR5（Write） | APP→PB03F | APP发送12字节控制指令 |
| CHAR6（Notify）| PB03F→APP | 下位机响应/状态原始包上报 |

数据格式：**与串口协议完全相同的12字节包**，APP直接解析串口协议即可。

---

## 三、串口协议摘要

波特率 19200，数据位8，无校验，1停止位（8N1）

```
[55AA][设备类型2B][命令码2B][数据区2B][校验码2B][55BB]
  校验码 = 命令码 + 数据区（取低16bit）
  设备类型: 0x8000=APP已连接, 0x0000=APP未连接
```

PB03F 每秒轮询下位机 8 条查询命令：
`0x0002→0x0003→0x0004→0x0005→0x0006→0x0007→0x0008→0x0009`，循环。

---

## 四、工程文件结构

```
example/ble_peripheral/bys_bridge/
├── main.c                        ← 硬件初始化 + app_main调用
├── DEV_PLAN.md                   ← 本文档
├── source/
│   ├── bys_bridge_Main.c         ← app_main()，OSAL启动
│   ├── OSAL_bys_bridge.c         ← OSAL任务表注册
│   ├── bys_bridge.h              ← 主任务事件宏 + 接口声明
│   ├── bys_bridge.c              ← 主任务：BLE状态机
│   ├── bys_uart.h                ← UART模块接口声明
│   └── bys_uart.c                ← UART模块：收发/解析/状态维护
└── （复用 simpleBlePeripheral 中）
    ├── source/sbpProfile_ota.h/.c ← GATT Profile (CHAR5 write / CHAR6 notify)
    ├── scatter_load.sct           ← 内存布局（复制过来）
    └── ram.ini                    ← Keil调试脚本（复制过来）
```

---

## 五、OSAL 任务表

| 优先级 | 任务名 | Init | ProcessEvent |
|--------|--------|------|--------------|
| 0（最高）| LL | — | LL_ProcessEvent |
| 1 | HCI | — | HCI_ProcessEvent |
| ... | GAP/L2CAP/SM/GATT | — | — |
| N-1 | GAPRole | — | GAPRole_ProcessEvent |
| N | BYS_Bridge | BYS_Bridge_Init | BYS_Bridge_ProcessEvent |

---

## 六、BLE 状态机

```
上电 → ADVERTISING（广播BYS格式）
           ↓ APP连接成功
       CONNECTED（启动1s轮询timer，透传双向数据）
           ↓ 连接断开 / 超时丢失
       ADVERTISING（自动重新广播，等待重连）
```

---

## 七、事件处理流程

| OSAL事件 | 触发时机 | 处理动作 |
|---------|---------|---------|
| `BYS_START_DEVICE_EVT` | Init后 | 启动GAP角色，开始广播 |
| `BYS_RESET_ADV_EVT` | 断连后100ms | 重启广播 |
| `BYS_POLL_TIMER_EVT` | 每1000ms | 向下位机发送下一条查询，重启timer |
| `BYS_UART_RX_EVT` | UART收到数据 | 解析响应，更新广播数据，Notify APP |

---

## 八、Keil 工程配置（新建步骤）

1. 复制 `simpleBlePeripheral` 整个目录，重命名为 `bys_bridge`
2. 删除 `source/` 下应用层 `.c/.h`，替换为本工程的6个新文件
3. 保留工程中所有 BLE 协议栈、OSAL、驱动层文件引用（不要修改）
4. 新增 `source/bys_uart.c` 和 `source/bys_bridge.c` 到 Keil 工程的 Application Group
5. `sbpProfile_ota.c` 保留在 Profile Group（路径引用 `../simpleBlePeripheral/source/`）
6. 预定义宏无需改动（保持 `PHY_MCU_TYPE=MCU_BUMBEE_M0` 等）

---

## 九、硬件引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| P9  | UART0 TX | DEBUG LOG 输出 |
| P10 | UART0 RX | DEBUG LOG 输入 |
| P23 UART1 TX | 发送给下位机 |
| P24 UART1 RX | 接收下位机数据 |
| P2/P3 | SWD | Keil 调试器（生产版去掉） |
