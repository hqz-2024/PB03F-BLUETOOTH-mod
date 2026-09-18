# BYS_Bridge V2 开发计划 —— A+B+App 三端互联

目前这个pb03f设备（称呼为A设备）。
我要在A设备的基础上，再增加一个B设备（使用PB03F作为主控）。
我使用手机app对A,B设备都进行匹配连接。再app端可以将A,B两个设备进行绑定。实现手机断开蓝牙，A与B设备依然能保持连接与通讯。

其中A设备为主要设备。在操作中一定要有A设备的参与，因为他是主要被控制的设备。
在三端共连的情况下，A设备能被手机APP和B设备访问并回复参数，同时B设备和手机APP也能对A设备进行操控，但是其中一端进行操控后，要同步数据给到另外一端。

如果B设备断联了或者本来就没有B设备。就app-A设备进行连接操作，与目前方案一样。

如果app断联了，就有A设备-B设备进行连接操作，但是app恢复连接后，要能同步数据给app端。

A设备本来参数也会有变化，变化之后要同步数据给到B设备与手机APP。

在实际应用场景里面就是：
A设备=电焊机（主要设备，有参数需要调整并需要同步给到手机和B设备（电焊面具））
B设备=电焊面具（辅助设备，需要同步A设备的参数，同时可以调整A设备的参数）


## 一、方案可行性评估

**结论：完全可行。** PHY62XX SDK 3.1.5 提供了两套现成的多连接机制，可以直接支撑本方案：

| 机制 | 库文件 | 适用设备 |
|------|--------|---------|
| Multi-Peripheral（纯从） | `ble_host_multi5.lib` + `peripheralMultiConn.c` | **A设备**（同时被App和B连接） |
| Multi-Role（主+从） | `ble_host_multi.lib` + `multi.c` | **B设备**（同时连A + 被App连） |

---

## 二、系统拓扑

```
┌────────────────────────────────────────────────────────────┐
│                      手机 APP                              │
│  连接A（读写控制）      连接B（配置绑定 / OTA）            │
└───────────┬─────────────────────────┬──────────────────────┘
            │ BLE                     │ BLE
            ▼                         ▼
  ┌─────────────────┐       ┌──────────────────┐
  │   A设备 (电焊机) │◄─────►│   B设备 (面具)    │
  │  Peripheral×2   │  BLE  │  Central+Peripheral│
  │  UART↔下位机    │       │  控制A，接受App   │
  └─────────────────┘       └──────────────────┘
```

- A设备：**双Peripheral**，同时接受App和B各一条连接
- B设备：**Multi-Role**，以Central身份连A，以Peripheral身份接受App连接
- 绑定后，手机断开时 A-B 链路独立维持

---

## 三、A设备改造方案（电焊机端）

### 3.1 工程配置变更

| 项目 | 变更前 | 变更后 |
|------|--------|--------|
| 连接库 | `ble_host.lib` | `ble_host_multi5.lib` |
| 角色文件 | `peripheral.c/h` | `peripheralMultiConn.c/h` |
| scatter | `peripheral.o(+RO)` | `peripheralMultiConn.o(+RO)` |
| 宏定义 | `MAX_NUM_LL_CONN=1` | `MAX_NUM_LL_CONN=2` |
| main.c | `BLE_MAX_ALLOW_CONNECTION=1` | `BLE_MAX_ALLOW_CONNECTION=2` |
| main.c | `ll_patch_slave()` | `ll_patch_multi()` |

### 3.2 连接句柄管理

`peripheralMultiConn.c` 维护 `gapRole_ConnectionHandle[2]` 数组。
应用层需区分哪个 handle 来自 App，哪个来自 B设备：

```c
// 通过对端MAC与绑定MAC比对来区分
#define CONN_INVALID  0xFFFF
static uint16 g_appConnHandle = CONN_INVALID;   // App连接句柄
static uint16 g_maskConnHandle = CONN_INVALID;  // B设备连接句柄
static uint8  g_boundMaskAddr[B_ADDR_LEN];      // 已绑定的B设备MAC（SNV存储）
```

在 `GAPROLE_CONNECTED` 回调中：

```c
// 读取对端MAC，与g_boundMaskAddr比较，分别赋值给两个handle变量
uint8 peerAddr[B_ADDR_LEN];
GAPRole_GetParameter(GAPROLE_CONN_BD_ADDR, peerAddr);
if (osal_memcmp(peerAddr, g_boundMaskAddr, B_ADDR_LEN))
    g_maskConnHandle = connHandle;
else
    g_appConnHandle  = connHandle;
```

### 3.3 数据同步（Shadow Register）

A设备持有参数镜像 `bys_state_t g_shadow`，任何一方修改后：

```c
// 参数被 App 写入 → 发给下位机 → 收到回包 → 同步给 B设备
// 参数被 B  写入 → 发给下位机 → 收到回包 → 同步给 App
// A自身参数变化  → 同步给 App 和 B设备

void bys_notify_all(uint8 *buf, uint8 len, uint16 src_handle) {
    if (g_appConnHandle  != CONN_INVALID && src_handle != g_appConnHandle)
        simpleProfile_Notify(g_appConnHandle,  buf, len);
    if (g_maskConnHandle != CONN_INVALID && src_handle != g_maskConnHandle)
        simpleProfile_Notify(g_maskConnHandle, buf, len);
}
```

---

## 四、B设备开发方案（面具端）

### 4.1 工程基础

基于 `example/ble_multi/simpleBleMultiConnection` 改造：

| 配置项 | 值 |
|--------|-----|
| `MAX_NUM_LL_CONN` | 2 |
| `MAX_CONNECTION_SLAVE_NUM` | 1（接受App连接） |
| `MAX_CONNECTION_MASTER_NUM` | 1（主动连接A） |
| `HOST_CONFIG` | `CENTRAL_CFG|PERIPHERAL_CFG` |

### 4.2 B设备主动连接A的流程

1. 上电读取 SNV 中存储的 A设备 MAC（`osal_snv_read`）
2. 若有有效 MAC → 直接调用 `GAPMultiRole_EstablishLink` 连接A
3. 若无 → 广播等待App连接，App下发绑定指令后存入SNV再连接

### 4.3 B设备 GATT 设计

B设备作为 Peripheral 时，提供以下特征值供 App 操作：

| 特征 | 方向 | 用途 |
|------|------|------|
| CHAR_BIND（Write） | App→B | 下发A设备MAC，触发绑定和连接 |
| CHAR_CTRL（Write） | App→B | 透传控制指令，B转发给A |
| CHAR_STATUS（Notify）| B→App | 将从A收到的状态数据转发给App |

B设备作为 Central 时，GATT Client 订阅A设备的 CHAR6 Notify，并可写 CHAR5。

---

## 五、绑定流程

```
1. App 先连接 A，获取 A 的 MAC 地址（通过广播包或读取特征值）
2. App 再连接 B，将 A 的 MAC 写入 B 的 CHAR_BIND 特征值
3. B 收到 MAC → osal_snv_write(SNV_ID_BOUND_A_ADDR, 6, macBuf)
4. B 立即发起 GAPMultiRole_EstablishLink 连接 A
5. A 在新连接回调中比对 MAC，识别为面具连接，赋值 g_maskConnHandle
6. A 也可以将 B 的 MAC 存入 SNV，下次优先向已知B设备开放连接
```

SNV ID 规划（避免与 BLE Stack 内置 ID 冲突，选 0x80 以上）：

| 设备 | SNV ID | 内容 |
|------|--------|------|
| B设备 | `0x80` | A设备 MAC（6字节） |
| A设备 | `0x81` | B设备 MAC（6字节，可选） |

---

## 六、断线重连策略

| 场景 | 行为 |
|------|------|
| App断开 | A重新广播；B保持与A的连接，缓存状态；App重连后A推送最新状态 |
| B断开 | A清空`g_maskConnHandle`，`g_appConnHandle`正常运作 |
| B重启 | B上电读SNV中A的MAC → 自动扫描连接A（无需App干预） |
| A重启 | A重新广播；B检测到连接断开 → 重新扫描连接A |

---

## 七、开发任务拆解

### 阶段一：A设备改造（Multi-Peripheral）

- [ ] 1. 复制 `bys_bridge` 工程，命名为 `bys_bridge_v2`
- [ ] 2. 替换库和角色文件，修改宏定义
- [ ] 3. 修改 `main.c`：`BLE_MAX_ALLOW_CONNECTION=2`，改用 `ll_patch_multi()`
- [ ] 4. 修改 `bys_bridge.c`：
  - [ ] 4a. 添加 `g_appConnHandle` / `g_maskConnHandle` 双句柄管理
  - [ ] 4b. 连接回调中通过MAC比对分类连接
  - [ ] 4c. 实现 `bys_notify_all()` 同步推送函数
  - [ ] 4d. CHAR5 WriteCallback 中调用 `bys_notify_all` 转发给另一端

### 阶段二：B设备新工程

- [ ] 5. 基于 `simpleBleMultiConnection` 新建 `bys_mask` 工程
- [ ] 6. 实现 GATT Service：CHAR_BIND / CHAR_CTRL / CHAR_STATUS
- [ ] 7. 实现 SNV 绑定存储：接收App写入的A设备MAC并持久化
- [ ] 8. 实现 Central 逻辑：上电扫描A → 连接 → 订阅A的Notify
- [ ] 9. 实现数据中继：
  - [ ] 9a. 收到A的Notify → 通过 CHAR_STATUS Notify 转发给App
  - [ ] 9b. 收到App的CHAR_CTRL Write → 通过GATT Client写入A的CHAR5

### 阶段三：联调

- [ ] 10. A+App 双连接验证（无B设备场景）
- [ ] 11. A+B 自动连接验证（无App场景）
- [ ] 12. A+B+App 三端同步验证
- [ ] 13. 异常场景验证：断线重连、单方掉线、参数同步一致性

---

## 八、关键风险与注意事项

| 风险 | 说明 | 应对 |
|------|------|------|
| BLE 时隙竞争 | 两条连接共享RF，连接间隔设置不当会导致丢包 | A设备两条连接均设 `minConnInterval=80`（100ms），减少碰撞 |
| 连接句柄区分 | 若B设备断联后重连，handle值可能变化 | 每次CONNECTED回调都重新比对MAC刷新handle |
| SNV写入时机 | 绑定时写SNV，切勿在中断或BLE回调深层写 | 写SNV操作发 OSAL 消息到任务层处理 |
| Notify拥塞 | 两端同时触发Notify可能超出发送队列 | 加发送状态标志，上一包ACK后再发下一包 |
| B设备Multi-Role复杂度 | Central+Peripheral同时运行，调度较复杂 | 严格参照 `simpleBleMultiConnection` 示例结构 |
