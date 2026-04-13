# PHY62XX BLE Mesh 开发指南

> v1.0 | PHY+ SW | 2021.03 | 适用于：PHY6222 / PHY6252

---

## 1 协议栈层次（EtherMind）

```
Model Layer                  ← 用户自定义 Model（Generic OnOff / Vendor / HSL / CTL）
Foundation Model Layer       ← 配置/健康模型（lib 内部）
Access Layer                 ← 数据格式化、加解密控制
Upper Transport Layer        ← 应用数据加解密 + Friendship 心跳
Lower Transport Layer        ← PDU 分段/重组
Network Layer                ← 消息地址、Relay/Proxy
Bearer Layer                 ← ADV Bearer / GATT Bearer
BLE Core                     ← Phyplus PHY/LL
```

---

## 2 Lib 库说明

| 库文件 | 功能 |
|--------|------|
| `libethermind_mesh_core.lib` | Mesh 协议栈核心（Provision / Config / 消息处理） |
| `libethermind_mesh_models.lib` | 标准 Model 实现（OnOff / HSL / CTL / Lightness / Scene） |
| `libethermind_utils.lib` | Mesh 存储（Flash 持久化） |
| `libethermind_ecdh.lib` | ECDH 加密（目前 SDK 未使用） |

---

## 3 工程目录结构

```
Project: ble_mesh
├── blemesh/
│   ├── ethermind/     ← Mesh 与 BLE 协议栈接口代码
│   ├── mesh_lib/      ← 以上 4 个 lib 库
│   ├── samples/       ← Mesh Model 回调与初始化（appl_sample_mesh_XXX.c）
│   └── phyplusmodel/  ← Mesh 配置模型接口
├── application/
│   ├── bleMesh.c      ← OSAL Task 定义
│   ├── bleMesh_Main.c ← main 入口
│   └── led_light.c    ← 应用逻辑
└── OSAL_bleMesh.c
```

---

## 4 Model 使能宏（`appl_sample_mesh_XXX.c`）

| 宏 | 说明 |
|----|------|
| `#define USE_HEALTH` | 启用 Health Model |
| `#define USE_HSL` | 启用 Light HSL Model |
| `#define USE_LIGHTNESS` | 启用 Light Lightness Model |
| `#define USE_CTL` | 启用 Light CTL Model |
| `#define USE_SCENE` | 启用 Light Scene Model |
| `#define USE_VENDORMODEL` | 启用 Vendor Model（自动启用 Easy Bonding） |
| `#undef USE_XXX` | 关闭对应 Model |

---

## 5 Mesh 初始化流程

```c
// appl_mesh_sample() 内部调用顺序：
BT_bluetooth_on()                        // 1. 初始化 BLE Host
MS_access_register_element(...)          // 2. 注册 Primary Element
UI_register_foundation_model_servers(element_handle)  // 3. 注册 Foundation Model
UI_register_generic_onoff_model_server(element_handle) // 4. 注册 Generic OnOff
UI_register_vendor_defined_model_server(element_handle) // 5. 注册 Vendor Model
blebrr_scan_pl(BLE_TRUE)                 // 6. 启动 ADV 扫描
MS_prov_start_unprovisioned_device_beacon()  // 7. 广播 Unprovisioned Beacon
```

---

## 6 常用 Provision API

| 函数 | 说明 |
|------|------|
| `appl_mesh_sample()` | Mesh 首次初始化（冷启动调用） |
| `UI_sample_reinit()` | 再次初始化（准备发 unprovision/proxy beacon，或获取 key） |
| `UI_sample_get_net_key()` | 获取当前 netkey |
| `UI_sample_get_device_key()` | 获取当前 device key |
| `UI_sample_check_app()` | 获取当前 appkey |
| `UI_set_relay_state(on)` | 开关 Relay 功能 |
| `UI_set_proxy_state(on)` | 开关 Proxy 功能 |
| `UI_set_friend_state(on)` | 开关 Friend 功能 |

---

## 7 常用 Access API

```c
// 获取当前节点单播地址
MS_access_cm_get_primary_unicast_address(&addr);

// 根据单播地址获取 element handle
MS_access_get_element_handle(elem_addr, &elem_handle);

// 根据 element handle + model ID 获取 model handle
MS_access_get_model_handle(elem_handle, model_id, &model_handle);
```

---

## 8 应用实例

### 8.1 Vendor Model 状态上报

```c
// 发送 Vendor 消息（服务端主动上报）
API_RESULT UI_vendor_model_send_status(uint8_t *data, uint16_t len)
{
    MS_ACCESS_MODEL_REQ_MSG_CONTEXT ctx;
    ctx.peer_addr = MS_UNASSIGNED_ADDR; // 广播
    MS_ACCESS_MODEL_STATE_PARAMS params;
    params.state_type = MS_STATE_VENDOR_T;
    params.state      = (void *)data;
    return MS_ACCESS_reply(&model_handle, &ctx, 0, 0, &params);
}

// 接收 Vendor 消息回调
static API_RESULT UI_vendor_model_server_cb(...)
{
    switch (opcode) {
        case MS_ACCESS_VENDOR_CMD_GET:   // 处理 Get
        case MS_ACCESS_VENDOR_CMD_SET:   // 处理 Set + 调用上报
        case MS_ACCESS_VENDOR_CMD_SET_UNACK:
        default: break;
    }
    return API_SUCCESS;
}
```

### 8.2 Generic OnOff 状态上报

```c
// 上报 OnOff 状态（供 access 层主动推送）
API_RESULT UI_generic_onoff_model_state_get(
    UINT16 state_t, UINT16 state_inst, void *param, UINT8 direction)
{
    MS_STATE_GENERIC_ONOFF_STRUCT *state = (MS_STATE_GENERIC_ONOFF_STRUCT *)param;
    state->onoff = current_onoff; // 填入当前状态
    return API_SUCCESS;
}

// SET 回调（由协议栈触发）
API_RESULT UI_generic_onoff_model_state_set(...)
{
    MS_STATE_GENERIC_ONOFF_STRUCT *new_state = ...;
    current_onoff = new_state->onoff;
    // 驱动实际输出（如 GPIO 控制灯）
    hal_gpio_write(LED_PIN, current_onoff);
    return API_SUCCESS;
}
```

---

## 9 常用配置宏

| 宏 | 说明 |
|----|------|
| `OSAL_CBTIMER_NUM_TASKS 1` | 内部 callback timer 数量（固定为 1，勿改） |
| `CFG_HEARTBEAT_MODE 0` | 关闭心跳 |
| `CFG_HEARTBEAT_MODE 1` | 开启心跳 |
| `CFG_PROXY_SERVER` | 开启 Proxy Server |
| `CFG_RELAY_SERVER` | 开启 Relay |
| `CFG_FRIEND_SERVER` | 开启 Friend |
