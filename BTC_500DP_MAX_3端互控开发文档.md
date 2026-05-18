# BTC500DP MAX 三端互控系统开发文档

## 一、系统总览

```
┌──────────────┐                       ┌──────────────────────────┐
│   遥控器端    │ ◄────BLE(BYS)──────► │       主设备端            │
│  ESP32-S3    │                       │  ┌────────┐  UART  ┌────┐│
│  + LCD       │                       │  │ PB03F  │◄──────►│电焊││
│              │                       │  │ (BYS)  │ 19200  │主控││
└──────────────┘                       │  └────────┘  8N0   └────┘│
       ▲                               └──────────────────────────┘
       │配置 MAC                              ▲
       │ (BLE BYS_remote)                     │ BLE(BYS)
       │                                      │
       └──────────────┐         ┌─────────────┘
                      │         │
                      ▼         ▼
                   ┌──────────────┐
                   │    APP 端     │
                   └──────────────┘
```

三端角色：

| 端 | 硬件 | BLE 角色 | BLE 名称 | 备注 |
|----|------|---------|---------|------|
| 主设备端 | PB03F + 电焊机主控 | Peripheral（同时承载 **2 个连接**） | `BYS` | 透传桥，UART ↔ BLE，对 App 与遥控器一视同仁 |
| App 端 | 手机 | Central | — | 业务连接已联调，新增遥控器配置入口 |
| 遥控器端 | ESP32-S3 + LCD | Central（正常模式）/ Peripheral（配置模式） | 配置模式：`BYS_remote` | 1:1 绑定主设备 MAC |

通讯链路：

- **App ↔ 主设备**：BLE 透传，承载 12 字节协议包
- **遥控器 ↔ 主设备**：BLE 透传，承载 12 字节协议包（与 App 完全一致的 Service / Characteristic / 数据格式）
- **主设备 ↔ 电焊机主控**：UART1（P24=TX，P23=RX），**19200 8N1**，同一 12 字节协议
- **App ↔ 遥控器**：BLE，仅用于初次将主设备 MAC 写入遥控器

**三端同时在线模型（核心）**：

- 主设备端将 `BLE_MAX_ALLOW_CONNECTION` 由 1 改为 **2**，允许 App 与遥控器同时连接。
- 任一上位机（A）的 Write 包 → 主设备校验后透传给电焊机主控。
- 电焊机主控的响应包 → 主设备 Notify **广播给所有当前已连接的上位机**（包括来源 A 自身，作为操作确认；其他上位机据此同步刷新 UI）。
- 同样地，电焊机主控主动上报（轮询 `0x008x` 系列）也 Notify 给所有上位机。
- 由此实现"任一端操作 → 三端 UI 即时同步"。

---

## 二、通讯协议复用

所有业务通讯（App↔主设备、遥控器↔主设备、主设备↔电焊机主控）均使用 `BTC_500DP_MAX_communication protocol.md` 中定义的 **12 字节定长包**：

```
[Header 2B] [设备类型 2B] [命令码 2B] [数据区 2B] [校验 2B] [Tail 2B]
校验 = 命令码 + 数据区
```

包头/包尾以 **现有量产工程 `bestarc_bluetoothmodel_BTC_500DP_PRO/source/bys_uart.h`** 为准：

```c
#define BYS_HEADER_0  0xAA   // 先发
#define BYS_HEADER_1  0x55
#define BYS_TAIL_0    0xBB
#define BYS_TAIL_1    0x55
```

设备类型字段：
- 上位机（App / 遥控器）→ 设备：`0x8000`（上位机已连接）/ `0x0000`（未连接）
- 设备 → 上位机：`0x0003`（BTC500DP MAX）

> 实现侧只要保持与现有量产固件字节序一致即可，命令码 / 数据区 / 电流范围 / 错误码 / 广播包结构沿用原协议，本文档不再重复。

### 2.1 三端对等数据源模型（关键）

三端均为**对等数据源**，任一端发起的变更都通过主控/PB03F 同步给另外两端：

| 数据源 | 触发方式 | 路径 |
|--------|---------|------|
| App 端 | 用户操作 App UI | App → BLE Write → PB03F → UART → 主控 → UART 响应 → PB03F → BLE Notify 到 App+遥控器 |
| 遥控器端 | 用户操作遥控器触屏 | 遥控器 → BLE Write → PB03F → UART → 主控 → UART 响应 → PB03F → BLE Notify 到 App+遥控器 |
| 主设备端 | 用户按主控面板按钮 | 主控 → UART 主动上报包 → PB03F → BLE Notify 到 App+遥控器 |

主控主动上报使用与查询响应相同的命令码（`0x008x` 系列），PB03F **不区分是"主控对查询的响应"还是"主控对本地按钮的主动上报"**，统一作为 Notify 包透传给所有已连接上位机。

`设备类型` 字段仅作为"是否有任意上位机在线"的纯指示，不影响主控功能：
- 主设备维护连接位图 `g_conn_mask`（bit0/bit1 分别对应 2 个连接槽）。
- PB03F 向主控发送的所有包（轮询查询 + App/遥控器 Write 透传）中：`g_conn_mask != 0` 填 `0x8000`，否则填 `0x0000`。
- 主控按钮在任何状态下均可操作，操作结果通过 UART 主动上报，PB03F 立即多播给所有上位机；当没有上位机在线时（`g_conn_mask == 0`），PB03F 仅刷新本地缓存与广播包，不做 Notify。

---

## 三、主设备端（PB03F + 电焊机主控）

### 3.1 功能职责

1. BLE Peripheral，广播名 `BYS`，广播包按协议 §四 组装（29 字节，含 MAC + 当前工作状态），由 `bys_bridge.c` 中 `advertData[]` 维护。
2. **同时承载 2 个 BLE 连接**（App + 遥控器），二者地位完全等价。
3. 任一连接 Write 12 字节包 → 校验 → 通过 UART1 透传给电焊机主控。
4. 电焊机主控 UART1 上报包 → 校验 → **Notify 给所有当前已连接的句柄**（实现"任一端操作三端同步"）。
5. 广播包中 8~21 字节（设备类型、钢板/网格、2T/4T、电流、后气、维弧、气压单位）随主控上报实时刷新，便于遥控器/App 在连接前即可看到最近状态。
6. 1 个连接占用时 **继续广播** 以接纳第 2 个连接；2 个连接都占满时停止广播；任一连接断开后立即恢复广播。

### 3.2 改造对照（基于 `bestarc_bluetoothmodel_BTC_500DP_PRO`）

| 项 | 原量产工程 | 升级后 |
|----|-----------|-------|
| `BLE_MAX_ALLOW_CONNECTION` | 1 | **2** |
| `pConnContext[]` / `g_pConnectionBuffer[]` | 按 1 算 | 按 2 重新分配 |
| `LL_LINK_HEAP_SIZE` | `(1*3+0)*280` | `(2*3+0)*280` |
| `LARGE_HEAP_SIZE` | 3 KB | **4 KB**（双连接 Host 内存增长） |
| Notify 分发 | 单 connHandle | 遍历 `linkDB_NumActive()` 所有活动 handle |
| 广播控制 | 连接后停广播 | 1 个连接时仍广播；2 个连接才停 |
| 设备类型字段 | `app_connected` 单标志 | `g_conn_mask`（位图），任一位为 1 即 `0x8000` |

### 3.3 软件模块（沿用并扩展现有文件）

| 模块 | 文件 | 关键改动 |
|------|------|---------|
| 入口 | `main.c` | `BLE_MAX_ALLOW_CONNECTION = 2`、堆/缓冲区扩容 |
| OSAL | `source/OSAL_bys_bridge.c` | 任务表不变 |
| Bridge 主逻辑 | `source/bys_bridge.c` | 增加多连接管理：连接/断开维护 `g_conn_mask[]` + `g_connHandle[2]`，Notify 时遍历下发；GAP 状态机在 `GAPROLE_CONNECTED` 时仍重启广播 |
| GATT Profile | `source/sbpProfile_ota.c/h` | UUID 不变；`simpleProfile_Notify` 内部改为对所有活动 handle 调用 `GATT_Notification` |
| UART 桥 | `source/bys_uart.c/h` | 不变，仍轮询 8 条查询；`bys_uart_poll_next(app_connected)` 改用 `g_conn_mask != 0` |

### 3.4 BLE GATT（沿用现有量产工程，不改 UUID）

来源：`source/sbpProfile_ota.h`

| 属性 | UUID | 属性 | 说明 |
|------|------|------|------|
| Service | `0xFFE0` | Primary | BYS 数据透传服务 |
| Char `SIMPLEPROFILE_CHAR1` | `0xFFE1` | **Write + Notify**，12 字节 | 双向数据通道，App 与遥控器均使用此特征收发协议包 |

遥控器端 Central 必须使用同一 Service/Char UUID 进行发现、Write Without Response 和订阅 Notify。

### 3.5 串口（沿用 `bys_uart.h` 配置）

| 项 | 取值 |
|----|------|
| Port | `UART1` |
| TX | `P24` |
| RX | `P23` |
| 波特率 | `19200` |
| 数据位/校验/停止 | 8N1（`parity = FALSE`） |
| 电源锁 | `hal_pwrmgr_lock(MOD_UART1)` 防止睡眠丢字节 |

> 注：协议文档原文写 "8N0"，实际现有量产固件 `uart_Cfg_t.parity = FALSE` 即 8N1，与电焊机主控已联调通过，沿用之。

数据流：
- BLE Write 收到 → 12 字节校验（包头/包尾/`校验=cmd+data`）→ `bys_uart_send_app_cmd()` 入队 → UART1 发出。
- UART1 RX 满包 → `bys_uart_process_rx()` 解析 → 调用 `rx_cb(raw_pkt)`：
  1. 更新 `g_bys_state` → 刷新 `advertData[16..29]`；
  2. 遍历 `g_connHandle[]`，对每个有效 handle 调 `GATT_Notification`。

### 3.6 主要参数

```c
/* main.c 关键修改 */
#define BLE_MAX_ALLOW_CONNECTION        2
#define BLE_MAX_ALLOW_PKT_PER_EVENT_TX  2
#define BLE_MAX_ALLOW_PKT_PER_EVENT_RX  2
#define BLE_PKT_VERSION                 BLE_PKT_VERSION_5_1
#define LARGE_HEAP_SIZE                 (4 * 1024)
g_system_clk    = SYS_CLK_XTAL_16M;
g_clk32K_config = CLK_32K_RCOSC;
g_rfPhyTxPower  = RF_PHY_TX_POWER_0DBM;
```

### 3.7 多连接管理伪代码

```c
#define MAX_LINK   2
static uint16 g_connHandle[MAX_LINK] = { GAP_CONNHANDLE_INIT, GAP_CONNHANDLE_INIT };
static uint8  g_conn_mask = 0;   /* bit0/bit1 分别表示两个槽是否占用 */

static int8 link_slot_alloc(uint16 h) {
    for (int8 i = 0; i < MAX_LINK; i++)
        if (g_connHandle[i] == GAP_CONNHANDLE_INIT) {
            g_connHandle[i] = h; g_conn_mask |= (1u << i); return i;
        }
    return -1;
}
static void link_slot_free(uint16 h) {
    for (int8 i = 0; i < MAX_LINK; i++)
        if (g_connHandle[i] == h) {
            g_connHandle[i] = GAP_CONNHANDLE_INIT; g_conn_mask &= ~(1u << i); break;
        }
}

/* GAP 状态回调 */
static void peripheral_state_cb(gaprole_States_t newState) {
    if (newState == GAPROLE_CONNECTED) {
        uint16 h; GAPRole_GetParameter(GAPROLE_CONNHANDLE, &h);
        link_slot_alloc(h);
        if (g_conn_mask != ((1u<<MAX_LINK)-1))
            GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &(uint8){TRUE});
    } else if (newState == GAPROLE_WAITING) {
        /* 某个连接断开，框架会回到 WAITING 并自动重启广播 */
    }
}

/* UART RX → Notify 给所有上位机 */
static void on_uart_rx(uint8 *pkt) {
    update_adv_state_fields(pkt);
    for (int8 i = 0; i < MAX_LINK; i++)
        if (g_connHandle[i] != GAP_CONNHANDLE_INIT)
            simpleProfile_NotifyTo(g_connHandle[i], SIMPLEPROFILE_CHAR1, BYS_PKT_LEN, pkt);
}
```

> `simpleProfile_NotifyTo()` 为新增 API：在原 `simpleProfile_Notify` 基础上接收外部 `connHandle` 参数。

---

## 四、遥控器端（ESP32-S3 + LCD）

基于 ESP-IDF 5.5.4 + LVGL v9。

### 4.1 运行模式

```
              ┌──────────────┐
              │  上电启动     │
              └──────┬───────┘
                     ▼
             读取 NVS: target_mac
                     │
        ┌────────────┴────────────┐
        │ 有 MAC                  │ 无 MAC
        ▼                         ▼
 ┌──────────────┐         ┌──────────────────┐
 │ 正常模式      │         │ 配置模式          │
 │ BLE Central  │         │ BLE Peripheral   │
 │ 主动连 MAC    │         │ 名: BYS_remote   │
 │ 扫主设备 BYS  │         │ 等待 App 写 MAC  │
 └──────┬───────┘         └────────┬─────────┘
        │                          │ App 写入 MAC
        │                          ▼
        │                  保存到 NVS → 重启
        │
        │ BOOT 5 次连按
        ▼
   清空 NVS MAC → 重启 → 进入配置模式
```

### 4.2 功能模块

| 模块 | 组件路径 | 职责 |
|------|---------|------|
| `remote_nvs` | `components/remote_nvs/` | 读写 `target_mac`（6 字节），命名空间 `bys_remote`，key `mac` |
| `remote_ble_central` | `components/remote_ble_central/` | 正常模式：扫描 → 按 MAC 过滤 → 连接 → 发现服务 → 订阅 Notify → Write |
| `remote_ble_peripheral` | `components/remote_ble_peripheral/` | 配置模式：广播 `BYS_remote`，提供 1 个 Write 特征接收 MAC |
| `remote_proto` | `components/remote_proto/` | 12 字节包封装/解析，与协议文档一致 |
| `remote_ui` | `components/remote_ui/` | LVGL UI：主界面（电流/模式/2T4T/后气/维弧/气压/状态/报警）+ 配置等待界面 |
| `remote_button` | `components/remote_button/` | BOOT 引脚检测，5 次连按（窗口 3s）触发 reset |
| `remote_app` | `main/` | 状态机：MODE_CONFIG / MODE_NORMAL 切换与编排 |

### 4.3 BLE 栈选择

强制使用 **NimBLE**（ESP-IDF 推荐，内存占用更低）：
- 配置模式：`esp_nimble_hci_init` + `ble_gap_adv_start`
- 正常模式：`ble_gap_disc` → `ble_gap_connect` → `ble_gattc_disc_all_svcs` → `ble_gattc_write_no_rsp` / Notify

### 4.4 配置模式 GATT（遥控器作为 Peripheral）

**沿用 App 已有规范，与主设备完全相同的 UUID**：

| 属性 | UUID | 属性 | 说明 |
|------|------|------|------|
| Service | `0xFFE0` | Primary | 配置/数据服务（同主设备） |
| Char | `0xFFE1` | Write + Notify，12 字节 | 配置时 App 在前 6 字节填入主设备 MAC，其余填 0；遥控器写入成功后 Notify 回 12 字节确认包，1s 内自动重启 |

配置写入包（12 字节，固定格式，便于 App 复用现有 BLE 通道）：

```
偏移  0  1  2  3  4  5    6  7    8  9   10 11
内容 [主设备MAC 6B]      [0x00FF]  [校验2B]  [BB 55]
                          ↑ 配置魔数（取代命令码）
校验 = 0x00FF + 0x0000  （数据区固定 0x0000，仅作占位）
```

> 配置魔数 `0x00FF` 是为了与正常业务命令码（`0x02xx`~`0x09xx`、`0x8xxx`）完全互斥，避免误识别。
> 包头沿用 `AA 55`，包尾沿用 `BB 55`，保持帧结构与业务协议一致。

遥控器收到合法配置包后：
1. 写 NVS：`bys_remote/mac` = 前 6 字节 MAC
2. 回 Notify 一包：`AA 55 | 00 03 | 00 FF | 00 01 | 01 03 | BB 55`（数据区 `0x0001` 表示"已保存"）
3. 1s 后 `esp_restart()`

### 4.5 正常模式连接流程

1. NimBLE 扫描，过滤广播包：
   - AD Type `0x09` Local Name == `"BYS"`，且
   - AD Type `0xFF` Manufacturer Data 前 6 字节 MAC 与 NVS 中 `target_mac` 完全一致。
2. 命中后停止扫描，`ble_gap_connect` 建立连接（即便此刻 App 已连接，主设备 `BLE_MAX_ALLOW_CONNECTION=2`，连接成功；若已是第 3 个连接则失败，自动退回扫描重试）。
3. 发现 Service `0xFFE0` / Char `0xFFE1`。
4. 订阅 Notify → 主设备 Notify 的所有上报包（含 App 操作回声）进入 UI 状态机更新。
5. UI 操作 → 组装 12 字节协议包 → Write Without Response。
6. 断线后 LVGL 顶栏切"未连接"图标，自动回到扫描状态，**无限重连**。

### 4.6 BOOT 5 次连按 reset

- BOOT 引脚使用 `iot_button` 组件，注册 `BUTTON_MULTIPLE_CLICK` 事件，`click_cnt = 5`，时间窗口 3 s。
- 触发后：
  1. `nvs_erase_key(handle, "mac")` + `nvs_commit`
  2. UI 切到配置模式提示页
  3. `esp_restart()`

### 4.7 UI 草案（LVGL v9）

- **配置模式**：全屏单行大字提示：
  ```
  请使用 app 进行初始化配置
  ```
  下方副标题显示遥控器自身 MAC（便于 App 端识别）+ 蓝牙图标动画。

- **正常模式**：
  - 顶栏：连接状态图标 + 报警图标
  - 左侧：模式（钢板/网格/除锈/气刨）+ 2T/4T 切换按钮
  - 中间：大号电流数字 + 加减按钮
  - 右侧：后气时间、维弧时间、气压单位
  - 底部：输入电压指示（120V/240V）

UI 每个可调参数对应一条 12 字节协议包（命令码 `0x0200`~`0x0700`），按下立即下发；UI 不做乐观更新，**完全以主设备 Notify 回来的 `0x82xx` 响应包为准**——这样 App 操作引起的状态变化也会通过同一 Notify 通道下发到遥控器 UI，实现三端一致。

---

## 五、App 端（已联调，新增配置遥控器入口）

App 业务连接（与主设备 `BYS`）保持不变，**新增**遥控器初始化配置流程：

### 5.1 配置流程

1. App 扫描到名为 `BYS_remote` 的设备 → 弹出"发现新遥控器，是否配置？"
2. 用户在 App 中选定一台 `BYS` 主设备（App 本地或当前列表中已知 MAC）。
3. App 连接 `BYS_remote` → 发现 Service `0xFFE0` / Char `0xFFE1` → 写入 12 字节配置包：
   ```
   AA 55 | 00 00 | 00 FF | 00 00 | FF 00 | BB 55
   ↑包头  ↑设备类型 ↑魔数   ↑数据   ↑校验   ↑包尾
   ```
   其中前 6 字节填写"主设备 MAC（小端）"——按现有 Manufacturer Data MAC 顺序原样填入。
4. 等待遥控器回 Notify，数据区 `0x0001` 即写入成功；提示"配置完成"。
5. 遥控器 1 s 后自动重启，进入正常模式连接主设备。

> 实际写入 12 字节的 byte0~byte5 填 MAC，byte6~byte7 填 `00 FF`（魔数），byte8~byte9 填 `00 00`，byte10~byte11 填校验和。可参考 §4.4 表格。

### 5.2 三端在线行为

- App 与遥控器同时连接到 `BYS` 后，App 任一操作通过现有 Write/Notify 通道下发；
- 主设备透传到主控，主控响应包被主设备 **Notify 给 App 和遥控器**；
- 因此 App 端 UI 应直接以 Notify 包驱动渲染，不再使用本地乐观更新（如果原 App 是乐观更新，建议改为"乐观+回声覆盖"）。

---

## 六、状态同步与冲突处理

- 主设备端 `BLE_MAX_ALLOW_CONNECTION = 2`，**App 与遥控器同时在线**；连接顺序无关。
- 任一上位机写入参数 → 主设备入队 UART → 主控回 `0x82xx` 响应 → 主设备 Notify 给两端 → 两端 UI 同步刷新。
- 主设备 8 条轮询查询（`0x008x`）持续运行，作为状态心跳；广播包内的状态字段也同步刷新，便于遥控器/App 在连接前即可显示最近状态。
- 错误包 `0x8100 / 数据区 0x0001`（设备工作中）由 UI 弹窗提示，用户稍后重试。
- 同帧并发：若 App 与遥控器几乎同时写入相同参数，UART 发送队列按 FIFO 排队，主控以最后一条为准；两端最终都会收到同一份 `0x82xx`，UI 收敛一致。
- 极端竞争（同参数相反方向）由产品层处理：当前协议无锁定机制，按"后写入者覆盖"语义即可。

---

## 七、NVS 数据结构（遥控器端）

| 命名空间 | Key | 类型 | 长度 | 说明 |
|---------|-----|------|------|------|
| `bys_remote` | `mac` | blob | 6 B | 绑定的主设备 MAC（小端，字节序与主设备广播 Manufacturer Data 中的 MAC 一致） |
| `bys_remote` | `cfg_ver` | u8 | 1 B | 配置版本号，预留升级用 |

---

## 八、开发里程碑

1. **M1 - 主设备端双连接改造**：`BLE_MAX_ALLOW_CONNECTION=2`，连接槽位管理，Notify 多播，1 连接时继续广播；用 2 部手机连接验证。
2. **M2 - 遥控器配置模式**：ESP32-S3 NimBLE Peripheral `BYS_remote`（UUID `0xFFE0/0xFFE1`）+ NVS 持久化 + 重启逻辑 + 配置模式 UI（"请使用 app 进行初始化配置"）。
3. **M3 - 遥控器正常模式**：扫描过滤（Name `BYS` + Manuf MAC 匹配）→ 连接 → GATT 透传 → 协议包收发。
4. **M4 - 遥控器 UI**：LVGL v9 主界面 + 报警/超时提示，所有状态由 Notify 包驱动。
5. **M5 - App 端增加配置入口**：扫描 `BYS_remote`，下发 12 字节配置包；调整 App 业务 UI 改为"以 Notify 回声为准"。
6. **M6 - BOOT 5 连按 reset** + 三端在线压力测试（双端高频写入、断连重连、Notify 回声去抖）。
