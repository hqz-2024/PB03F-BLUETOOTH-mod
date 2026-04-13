# PHY622X PWR_MGR 电源管理应用指南

> v1.0 | phyplusinc | 2021.03.17 | 代码路径：`components/driver/pwrmgr/pwrmgr.c(.h)`

---

## 1 功耗模式总览

| 模式 | CPU | 外设 | RAM 保持 | 唤醒源 | 唤醒后状态 |
|------|-----|------|---------|--------|-----------|
| **普通模式** | 全速 | 全速 | — | — | — |
| **CPU 休眠** | 休眠 | 运行 | 全部 | 中断/事件 | 继续运行（OS 自动控制） |
| **深度休眠** | 休眠 | 大部分关 | 可配 SRAM0/1/2 | GPIO / RTC | 恢复上下文，调用唤醒回调 |
| **Standby** | 休眠 | 全关 | 仅 RAM0 | GPIO | 系统 Reset（`wakeupProcess_standby` 处理） |
| **关机** | 休眠 | 全关 | 无 | GPIO | 系统重启，上下文丢失 |

**深度休眠流程：**
IDLE → `sleep_process()` → 回调各模块 `sleep_handler()` → 休眠 → IO/RTC 唤醒 → 初始化 → 回调各模块 `wakeup_handler()` → OSAL 调度

**Standby 流程：**
APP 调用 `hal_pwrmgr_enter_standby()` → 休眠 → IO 唤醒 → `wakeupProcess_standby()` → 满足条件则 System Reset

---

## 2 数据结构

### MODULE_e（`mcu_phy_bumbee.h`）

```c
typedef enum {
    MOD_NONE = 0,
    MOD_DMA  = 3,   MOD_AES   = 4,   MOD_IOMUX = 7,
    MOD_UART0= 8,   MOD_I2C0  = 9,   MOD_I2C1  = 10,
    MOD_SPI0 = 11,  MOD_SPI1  = 12,  MOD_GPIO  = 13,
    MOD_QDEC = 15,  MOD_ADCC  = 17,  MOD_PWM   = 18,
    MOD_SPIF = 19,  MOD_VOC   = 20,
    MOD_TIMER5 = 21, MOD_TIMER6 = 22, MOD_UART1 = 25,

    // CP 侧（偏移 +32）
    MOD_CP_CPU = 32, MOD_BB = 35, MOD_TIMER = 36,
    MOD_WDT = 37,    MOD_COM = 38,  MOD_KSCAN = 39,
    MOD_BBREG = 41,  MOD_TIMER1 = 53, MOD_TIMER2 = 54,
    MOD_TIMER3 = 55, MOD_TIMER4 = 56,

    // 应用层可用（偏移 +96）
    MOD_USR0 = 96, MOD_USR1, MOD_USR2, MOD_USR3,
    MOD_USR4, MOD_USR5, MOD_USR6, MOD_USR7, MOD_USR8,
} MODULE_e;
```

> **应用代码使用 `MOD_USR0 ~ MOD_USR8`，避免占用系统模块 ID。**

### pwrmgr_Ctx_t

```c
typedef struct {
    MODULE_e     module_id;
    bool         lock;           // TRUE=禁止休眠，FALSE=允许休眠
    pwrmgr_Hdl_t sleep_handler;  // 进入休眠前回调
    pwrmgr_Hdl_t wakeup_handler; // 唤醒后回调
} pwrmgr_Ctx_t;  // 最多注册 10 个模块
```

### pwroff_cfg_t（关机/Standby 唤醒源配置）

```c
typedef struct {
    gpio_pin_e      pin;
    gpio_polarity_e type; // POL_FALLING 或 POL_RISING
} pwroff_cfg_t;
```

---

## 3 API 说明

| 函数 | 说明 |
|------|------|
| `hal_pwrmgr_init()` | 模块初始化，系统启动时调用 |
| `hal_pwrmgr_register(mod, sleepHdl, wakeupHdl)` | 注册模块及休眠/唤醒回调 |
| `hal_pwrmgr_unregister(mod)` | 取消注册 |
| `hal_pwrmgr_lock(mod)` | 禁止该模块允许的休眠 |
| `hal_pwrmgr_unlock(mod)` | 允许该模块休眠 |
| `hal_pwrmgr_is_lock(mod)` | 查询 lock 状态 |
| `hal_pwrmgr_RAM_retention(sram)` | 配置保持的 RAM 区域，可选 `RET_SRAM0\|RET_SRAM1\|RET_SRAM2` |
| `hal_pwrmgr_RAM_retention_set()` | 使能 RAM retention |
| `hal_pwrmgr_RAM_retention_clr()` | 清除 RAM retention |
| `hal_pwrmgr_clk_gate_config(mod)` | 配置唤醒时需使能的时钟 |
| `hal_pwrmgr_poweroff(pcfg, pin_num)` | 进入关机模式（配置唤醒 GPIO） |
| `hal_pwrmgr_enter_standby(pcfg, pin_num)` | 进入 Standby 模式 |

---

## 4 低功耗使用步骤

```c
// 1. 工程宏：CFG_SLEEP_MODE=PWR_MODE_SLEEP
// 2. 系统初始化时：
hal_pwrmgr_init();
hal_pwrmgr_RAM_retention(RET_SRAM0 | RET_SRAM1);

// 3. 注册模块（APP Task 初始化时）：
hal_pwrmgr_register(MOD_USR0, sleep_cb, wakeup_cb);

// 4. 运行中禁止/允许休眠（如正在收发数据时）：
hal_pwrmgr_lock(MOD_USR0);
// ... 忙碌操作 ...
hal_pwrmgr_unlock(MOD_USR0);
```

**建议：**
- `sleep_cb()`：配置唤醒引脚的触发极性（根据当前电平设置边沿方向）
- `wakeup_cb()`：判断唤醒源，重新初始化相关外设

---

## 5 功耗评估模型

一个休眠唤醒周期由 5 段组成：

| 阶段 | 说明 | 主要影响因素 |
|------|------|------------|
| X1 休眠 | 系统处于深度休眠 | RAM retention 数量越多，电流越大 |
| X2 唤醒 | 唤醒到 HCLK 切换前 | 时钟源（32MHz RC），相对固定 |
| X3 工作 | HCLK 已切换，非射频阶段 | HCLK 频率、LowCurrentLDO 状态 |
| X4 RF TX | 射频发送 | PA 发射功率 |
| X5 RF RX | 射频接收 | 相对固定 |

**平均功耗计算：**

```
总功耗 = I_sleep×X1 + I_wakeup×X2 + I_work×(X3+X4+X5) + I_rf×(X4+X5)
平均功耗 = 总功耗 / (X1+X2+X3+X4+X5)
电池寿命 = 电池容量(mAh) × 3600 / 平均功耗(mA) （秒）
```

**simpleBlePeripheral advertising 测算参考（500ms 广播间隔，3组收发）：**

| HCLK | 唤醒电流 | RF 电流 | 工作电流 | 休眠电流 | 平均功耗 |
|------|---------|---------|---------|---------|---------|
| 64MHz | 3.07mA | 3.65mA | 2.55mA | 5.8µA | ~0.046mA |
| 48MHz | 3.03mA | 3.60mA | 2.26mA | 5.6µA | ~0.044mA |
| 16MHz | 3.03mA | 3.60mA | 1.65mA | 5.6µA | ~0.039mA |

---

## 6 系统启动时间

| 启动类型 | 时间 | 说明 |
|---------|------|------|
| 冷启动 | ~91ms | 首次上电，ROM code 完成初始化 |
| 软启动 | ~44ms | Software reset，可绕过 DWC（约省 50ms） |
| 唤醒 | ~3.4ms | 从 SRAM 启动，无 Flash 搬运，最快 |
| main → BLE 初始化完成 | ~31ms | simpleBLEPeripheral 参考值 |
