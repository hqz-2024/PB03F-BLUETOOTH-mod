---
name: pb03f
description: PHY62XX (PB03F) BLE SoC SDK 3.1.5 开发知识库，涵盖工程结构、驱动API、BLE栈、OSAL任务模型、低功耗管理等核心开发要点。
---

# PB03F / PHY62XX SDK 开发技能库

## 一、芯片与SDK概述

- **芯片**：PHY6222 / PHY6252（PB03F封装）
- **内核**：ARM Cortex-M0
- **SDK版本**：PHY62XX_SDK_3.1.5
- **开发工具**：Keil MDK（主要）/ GCC（部分示例支持）
- **烧录方式**：SWD（P02/P03）或 XIP Flash

芯片集成 BLE 5.1 射频，内置 Flash（XIP 执行），SRAM 分三块：
- SRAM0：32KB，地址 `0x1fff0000`~`0x1fff7fff`
- SRAM1：16KB，地址 `0x1fff8000`~`0x1fffbfff`
- SRAM2：16KB，地址 `0x1fffc000`~`0x1fffffff`

---

## 二、工程目录结构

```
SDK根目录/
├── components/
│   ├── arch/          # 架构相关（启动、中断向量）
│   ├── ble/           # BLE协议栈头文件（controller/hci/host/include）
│   ├── driver/        # 片上外设驱动
│   ├── gcc/           # GCC链接脚本等
│   ├── inc/           # 公共头文件（types.h, bus_dev.h等）
│   ├── keil/          # Keil分散加载文件
│   ├── libraries/     # 工具库（fs, crypto, cli, crc16等）
│   ├── osal/          # 操作系统抽象层
│   └── profiles/      # BLE Profile（角色、服务、OTA等）
├── example/
│   ├── peripheral/    # 纯外设Demo（GPIO/UART/SPI/ADC/Timer等）
│   ├── ble_peripheral/# BLE从设备Demo（simpleBLE/HID/pwmLight等）
│   ├── ble_central/   # BLE主设备Demo
│   ├── ble_multi/     # 多角色Demo（主从并存，最多5连接）
│   ├── ble_mesh/      # BLE Mesh Demo（light/switch/gateway/lpn等）
│   ├── OTA/           # OTA升级Demo
│   └── PhyPlusPhy/    # 2.4G私有协议Demo
├── lib/               # 预编译库（rf.lib/ble_host.lib/mesh.lib等）
└── misc/              # ROM符号表、jump_table等
```

---

## 三、驱动层 API（`components/driver/`）

### 3.1 GPIO（`gpio/gpio.h`）

**PIN枚举**：`gpio_pin_e`，命名规则 `GPIO_P00`~`GPIO_P34`，别名 `P0`~`P34`

```c
// 初始化GPIO模块（程序启动时调用一次）
hal_gpio_init();

// 配置为输入/输出
hal_gpio_pin_init(P9, GPIO_OUTPUT);   // OEN = GPIO_OUTPUT
hal_gpio_pin_init(P10, GPIO_INPUT);   // IE  = GPIO_INPUT

// 写/读
hal_gpio_write(P9, 1);       // 拉高
hal_gpio_write(P9, 0);       // 拉低
bool val = hal_gpio_read(P10);

// 设置上下拉
hal_gpio_pull_set(P10, GPIO_PULL_UP);    // GPIO_FLOATING / GPIO_PULL_UP_S / GPIO_PULL_UP / GPIO_PULL_DOWN

// 复用功能配置
hal_gpio_fmux_set(P9,  FMUX_UART0_TX);
hal_gpio_fmux_set(P10, FMUX_UART0_RX);
hal_gpio_fmux(P9, Bit_ENABLE);   // 启用复用

// GPIO中断（边沿触发）
typedef void (*gpioin_Hdl_t)(gpio_pin_e pin, gpio_polarity_e type);
hal_gpioin_register(P7, posedge_cb, negedge_cb);
hal_gpioin_enable(P7);
hal_gpioin_disable(P7);
hal_gpioin_unregister(P7);

// 模拟IO配置（P11/P14~P20/P23~P25 支持模拟）
hal_gpio_cfg_analog_io(P11, Bit_ENABLE);
```

### 3.2 UART（`uart/uart.h`）

```c
uart_Cfg_t cfg = {
    .tx_pin      = P9,
    .rx_pin      = P10,
    .rts_pin     = GPIO_DUMMY,
    .cts_pin     = GPIO_DUMMY,
    .baudrate    = 115200,
    .use_fifo    = TRUE,
    .hw_fwctrl   = FALSE,
    .use_tx_buf  = FALSE,
    .parity      = FALSE,
    .evt_handler = uart_rx_cb,  // void uart_rx_cb(uart_Evt_t* pev)
};
hal_uart_init(cfg, UART0);   // UART0 或 UART1

// 发送
hal_uart_send_byte(UART0, 0xAB);
hal_uart_send_buff(UART0, buf, len);

// 回调事件类型
// pev->type == UART_EVT_TYPE_RX_DATA        接收完成
// pev->type == UART_EVT_TYPE_RX_DATA_TO     接收超时
// pev->type == UART_EVT_TYPE_TX_COMPLETED   发送完成
```

### 3.3 I2C（`i2c/i2c.h`）

```c
#include "i2c.h"
hal_i2c_init(I2C_0, I2C_CLOCK_400K, P2, P3, i2c_evt_cb);
hal_i2c_tx_start(I2C_0, slave_addr, buf, len, I2C_STOP);
hal_i2c_rx_start(I2C_0, slave_addr, buf, len, I2C_STOP);
hal_i2c_deinit(I2C_0);
```

### 3.4 SPI（`spi/spi.h`）

```c
#include "spi.h"
spi_Cfg_t spi_cfg = {
    .sclk_pin = P14,
    .ssn_pin  = P15,
    .MOSI     = P16,
    .MISO     = P17,
    .baudrate = 4000000,
    .spi_tmod = SPI_TRXD,
    .spi_scph = SPI_SCPH_1EDGE,
    .spi_scpol= SPI_SCPOL_HIGH,
    .spi_dfs  = SPI_8BIT,
    .force_cs_ctrl = FORCE_CS_IDLE,
    .evt_handler = spi_evt_cb,
};
hal_spi_init(SPI0, spi_cfg);
hal_spi_send_and_recv(SPI0, tx_buf, tx_len, rx_buf, rx_len);
```

### 3.5 ADC（`adc/adc.h`）

```c
hal_adc_init();
adc_Cfg_t adc_cfg = {
    .channel           = ADC_CH0,     // ADC_CH0~ADC_CH9, ADC_CH_VOICE
    .is_continue_mode  = FALSE,
    .is_differential_mode = 0,        // 0=单端, 非0=差分
    .is_high_resolution   = 1,
};
hal_adc_config_channel(adc_cfg, adc_data_cb);
hal_adc_clock_config(HAL_ADC_CLOCK_320K);
hal_adc_start(POLLING_MODE);   // POLLING_MODE / INTERRUPT_MODE / CCOMPARE_MODE

// 回调: void adc_data_cb(adc_Evt_t* pev)
// pev->type == HAL_ADC_EVT_DATA
// 结果转电压：double v = hal_adc_value_cal(ch, pev->data, pev->size, high_res, diff_mode);

hal_adc_stop();
hal_adc_deinit();
```

**ADC通道与引脚对应**：
| 通道 | 引脚 |
|------|------|
| ADC_CH0 (单端) | P11 |
| ADC_CH1 (单端) | P23 |
| ADC_CH2 (单端) | P24 |
| ADC_CH3 (单端) | P14 |
| ADC_CH4 (单端) | P15 |
| ADC_CH9 (单端) | P20 |
| ADC_CH1DIFF    | P23(+)/P11(-) |
| ADC_CH2DIFF    | P14(+)/P24(-) |
| ADC_CH3DIFF    | P20(+)/P15(-) |

### 3.6 Timer（`timer/timer.h`）

```c
#include "timer.h"
// AP_TIM1~AP_TIM6, 默认 TIM1/TIM2 被BLE协议栈占用，用户用 TIM3~TIM6
hal_timer_init(AP_TIM3);
hal_timer_set(AP_TIM3, 1000);   // 单位：us
hal_timer_start(AP_TIM3);
hal_timer_stop(AP_TIM3);
hal_timer_deinit(AP_TIM3);
// 中断处理：实现 void hal_TIM3_IRQHandler(void) __attribute__((weak));
```

### 3.7 PWM（`pwm/pwm.h`）

```c
#include "pwm.h"
// PWM0~PWM5，复用到任意GPIO
hal_pwm_init(PWM_CH0, 1000 /*Hz*/, 50 /*duty%*/, P11);
hal_pwm_start(PWM_CH0);
hal_pwm_stop(PWM_CH0);
hal_pwm_set_param(PWM_CH0, 2000, 80);
```

### 3.8 DMA（`dma/dma.h`）

```c
#include "dma.h"
hal_dma_init();
// DMA通道通常由UART/SPI驱动内部使用，直接配置uart_Cfg_t/spi_Cfg_t即可触发DMA
```

### 3.9 Flash（`flash/flash.h`）

```c
#include "flash.h"
// 内置Flash基地址 0x11000000，扇区4KB，支持XIP执行
hal_flash_write(addr, buf, len);
hal_flash_read(addr, buf, len);
hal_flash_erase_sector(addr);      // 擦除4KB
hal_flash_erase_block64(addr);     // 擦除64KB
// 加载MAC地址（出厂写入）
uint8_t mac[6];
hal_flash_get_mac_address(mac);

// Flash保护（FLASH_PROTECT_FEATURE=1时使用）
hal_flash_enable_lock(MAIN_INIT);
```

### 3.10 Watchdog（`watchdog/watchdog.h`）

```c
#include "watchdog.h"
watchdog_config(WDG_2S);    // WDG_2S / WDG_4S / WDG_8S
hal_watchdog_feed();        // 在OSAL idle任务中自动喂狗（已集成）
```

### 3.11 日志输出（`log/log.h`）

```c
#include "log.h"
LOG_INIT();              // 初始化日志（main.c的hal_init()中调用）
LOG("value=%d\n", val); // 通过UART0输出，默认波特率115200
LOG_DUMP_BYTE(buf, len); // 十六进制dump
```

### 3.12 电源管理（`pwrmgr/pwrmgr.h`）

```c
#include "pwrmgr.h"
hal_pwrmgr_init();   // 初始化，在hal_init()中调用

// 模块锁定/解锁睡眠（持有锁期间芯片不进入睡眠）
hal_pwrmgr_lock(MOD_USR0);
hal_pwrmgr_unlock(MOD_USR0);

// 注册睡眠/唤醒回调
hal_pwrmgr_register(MOD_USR0, sleep_cb, wakeup_cb);

// SRAM保留配置（睡眠期间保持数据）
hal_pwrmgr_RAM_retention(RET_SRAM0 | RET_SRAM1);
hal_pwrmgr_RAM_retention_set();

// 低功耗LDO
hal_pwrmgr_LowCurrentLdo_enable();

// 进入关机/Standby模式（GPIO唤醒）
pwroff_cfg_t wakeup_pin = { .pin = P7, .type = POL_RISING, .on_time = 10 };
hal_pwrmgr_poweroff(&wakeup_pin, 1);
hal_pwrmgr_enter_standby(&wakeup_pin, 1);
```

**睡眠模式配置**（`main.c` 中）：
```c
// CFG_SLEEP_MODE == PWR_MODE_SLEEP  → 深度睡眠（BLE定时唤醒）
// CFG_SLEEP_MODE == PWR_MODE_NO_SLEEP → 不睡眠
```

---

## 四、OSAL 任务系统

### 4.1 初始化流程（固定模式）

```c
// 1. app_main() 入口
int app_main(void) {
    osal_init_system();
    osal_pwrmgr_device(PWRMGR_BATTERY);
    osal_start_system();  // 不返回
    return 0;
}

// 2. OSAL_<AppName>.c 中注册任务
const pTaskEventHandlerFn tasksArr[] = {
    LL_ProcessEvent,
    Hci_ProcessEvent,
    GAPRole_ProcessEvent,
    GAPBondMgr_ProcessEvent,
    GAP_ProcessEvent,
    SM_ProcessEvent,
    GATT_ProcessEvent,
    ATT_ProcessEvent,
    L2CAP_ProcessEvent,
    GATTServApp_ProcessEvent,
    DevInfo_ProcessEvent,
    SimpleProfile_ProcessEvent,
    Batt_ProcessEvent,
    OSAL_user_task_ProcessEvent,  // 用户任务放最后
};
```

### 4.2 用户任务模板

```c
// 头文件声明
uint8 MyTask_TaskID;
void  MyTask_Init(uint8 task_id);
uint16 MyTask_ProcessEvent(uint8 task_id, uint16 events);

// 初始化（注册任务后自动调用）
void MyTask_Init(uint8 task_id) {
    MyTask_TaskID = task_id;
    // 启动定时器：1000ms后触发事件
    osal_start_timerEx(MyTask_TaskID, MY_PERIODIC_EVT, 1000);
}

// 事件处理
uint16 MyTask_ProcessEvent(uint8 task_id, uint16 events) {
    if (events & MY_PERIODIC_EVT) {
        // 业务逻辑
        osal_start_timerEx(MyTask_TaskID, MY_PERIODIC_EVT, 1000); // 重启定时器
        return (events ^ MY_PERIODIC_EVT);
    }
    return 0;
}
```

### 4.3 OSAL 常用API

```c
// 定时器
osal_start_timerEx(task_id, event, timeout_ms);
osal_stop_timerEx(task_id, event);
osal_get_timeoutEx(task_id, event);  // 返回剩余时间ms

// 内存
uint8* p = osal_mem_alloc(size);
osal_mem_free(p);

// 消息
osal_event_hdr_t* msg = (osal_event_hdr_t*)osal_msg_allocate(sizeof(osal_event_hdr_t));
msg->event = MY_MSG_EVT;
osal_msg_send(task_id, (uint8*)msg);
// 在ProcessEvent中接收：
uint8* msg = osal_msg_receive(task_id);
osal_msg_deallocate(msg);

// 发送事件（中断上下文使用）
osal_set_event(task_id, event_flag);
```

---

## 五、BLE 协议栈

### 5.1 main.c 关键配置

```c
// 连接数配置
#define BLE_MAX_ALLOW_CONNECTION    1    // 最大连接数（multi role最多5）
#define BLE_MAX_ALLOW_PKT_PER_EVENT_TX  2
#define BLE_MAX_ALLOW_PKT_PER_EVENT_RX  2
#define BLE_PKT_VERSION  BLE_PKT_VERSION_5_1  // 或 BLE_PKT_VERSION_4_0

// Heap配置
#define LARGE_HEAP_SIZE  (3*1024)   // 有OBSERVER时用4KB

// 时钟配置
g_system_clk = SYS_CLK_XTAL_16M;   // 常用：16M/32M(DBL)/48M(DLL)/64M(DLL)/96M(DLL)
g_clk32K_config = CLK_32K_RCOSC;   // 或 CLK_32K_XTAL（需外部32.768K晶体）

// RF发射功率
g_rfPhyTxPower = RF_PHY_TX_POWER_N2DBM;  // -20~+6dBm可选

// ll patch（必须与角色匹配）
ll_patch_slave();       // 从设备
ll_patch_advscan();     // 纯广播/扫描（OBSERVER_CFG）
ll_patch_sleep();       // 深度睡眠（CFG_SLEEP_MODE==PWR_MODE_SLEEP时）
ll_patch_no_sleep();    // 不睡眠
```

### 5.2 BLE 角色 Profile

**从设备角色**（`profiles/Roles/peripheral.h`）：
```c
#include "peripheral.h"
static gapRolesCBs_t myApp_roleCBs = {
    .pfnStateChange = peripheralStateNotificationCB,
    .pfnRssiRead    = NULL,
    .pfnParamUpdate = NULL,
};
GAPRole_StartDevice(&myApp_roleCBs);
```

**主设备角色**（`profiles/Roles/central.h`）：
```c
#include "central.h"
GAPCentralRole_StartDevice(&myApp_roleCBs);
GAPCentralRole_StartDiscovery(DEFAULT_DISCOVERY_MODE, TRUE, FALSE);
GAPCentralRole_EstablishLink(FALSE, FALSE, peer_addrType, peer_addr);
```

**多角色**（`profiles/multiRole/multi.h`）：
```c
#include "multi.h"
// 最多2 slave + 3 master，共5连接
GAPMultiRole_StartDevice(&myApp_roleCBs);
```

### 5.3 GATT 服务注册

```c
// 以 SimpleProfile 为例（components/profiles/SimpleProfile/）
#include "simpleProfile.h"
SimpleProfile_AddService(GATT_ALL_SERVICES);
SimpleProfile_RegisterAppCBs(&simpleBLEPeripheral_SimpleProfileCBs);

// 发送Notification
GATTServApp_ProcessCharCfg(simpleProfileCharCfgTable, &charValue,
                            FALSE, simpleProfileAttrTbl,
                            GATT_NUM_ATTRS(simpleProfileAttrTbl),
                            INVALID_TASK_ID);
```

### 5.4 OTA

```c
// 使用 components/profiles/ota/ 下的服务
#include "ota_service.h"
OTA_AddService();
// scatter_load.sct 中需保留OTA区域
// 双区OTA：APP区 + OTA区，通过SLB引导
```

---

## 六、BLE Mesh

```c
// lib/ 提供 mesh.lib（Keil）/ libphy6222_host.a（GCC）
// 示例：example/ble_mesh/mesh_light、mesh_switch、mesh_gateway 等
// 支持 ali Genie 接入（aliGenie_bleMesh/）

// Mesh 使用 ethermind 协议栈（components/ethermind/）
// 主要模型：Generic OnOff、Generic Level、Light Lightness 等
// 节点类型：provisioner(gateway) / device(node) / LPN / Friend
```

---

## 七、文件系统（FS）

```c
// 使用 components/libraries/fs/
#include "fs.h"
// 基于 Flash 实现 KV 存储，支持 CRC 校验、双备份
fs_init(FLASH_UCDS_ADDR_BASE, 0x1000 /*size*/);
fs_write(FS_KEY_ID, data, len);
fs_read(FS_KEY_ID, buf, len);
fs_remove(FS_KEY_ID);
```

---

## 八、加密库

```c
// components/libraries/crypto/ 和 tinycrypt-0.2.8/
// 支持 AES-128/256、AES-CCM、AES-CMAC、SHA-256、ECDH P-256

// components/libraries/crc16/
#include "crc16.h"
uint16_t crc = crc16(data, len);
```

---

## 九、新建工程流程（Keil）

1. 复制 `example/ble_peripheral/simpleBlePeripheral/` 为模板
2. 修改 `scatter_load.sct` 中的内存分布（XIP Flash 段 / SRAM 段）
3. `main.c` 中调整：
   - `BLE_MAX_ALLOW_CONNECTION`（连接数）
   - `LARGE_HEAP_SIZE`（堆大小）
   - `g_system_clk`（系统时钟）
   - `g_clk32K_config`（32K时钟源）
   - `g_rfPhyTxPower`（发射功率）
4. `OSAL_xxx.c` 中注册任务表（`tasksArr[]`）
5. 实现用户任务 `xxx_Init()` 和 `xxx_ProcessEvent()`
6. Keil 工程选项：
   - Device：ARMCM0
   - 添加必要 lib：`lib/ble_host.lib`、`lib/rf.lib`（或 `rf_light.lib`）
   - 头文件路径包含：`components/inc`、`components/osal/include`、`components/ble/include`、`components/driver/xxx`
   - 宏定义：`PHYPLUS_GCC`（GCC时）或无（Keil时）

---

## 十、调试

- **SWD调试**：使用 Keil 连接 P02(SWDCLK)/P03(SWDIO)，加载 `ram.ini`（RAM调试）或 `ram_xip.ini`（XIP调试）
- **日志**：UART0 (P09=TX, P10=RX), 115200bps, `LOG("...")`
- **ROM符号**：`misc/bb_rom_sym_m0.gdbsym` 用于GDB调试时加载ROM函数符号
- **DTM测试**：P20=HIGH 时进入直接测试模式（`rf_phy_direct_test()`）
- **hardfault**：`misc/jump_table.c` 中有 `_hard_fault` 处理（输出PC/LR等）

---

## 十一、预编译库说明

| 库文件 | 用途 |
|--------|------|
| `rf.lib` / `libphy6222_rf.a` | BLE射频控制器（LL层），完整功能 |
| `rf_light.lib` | 精简射频库，从设备省SRAM |
| `rf_mst.lib` | 主设备/Mesh专用射频库 |
| `ble_host.lib` / `libphy6222_host.a` | BLE Host层（HCI/L2CAP/ATT/GATT/SM/GAP）|
| `mesh.lib` | BLE Mesh协议栈（EtherMind） |
| `phy_font.lib` | 字库（UI相关） |
| `libphy6222_sec_boot.a` | 安全启动（SLB） |

---

## 十三、附加资料索引

> 以下文件位于 `.augment/skills/pb03f/docs/` 目录下，需要查阅时通过 `view` 工具读取完整内容。

| 文件 | 主题 | 关键内容速查 |
|------|------|------------|
| `docs/PHY6222 BLE SoC 数据手册.md` | 芯片数据手册 | QFN32/QFN24引脚定义、内存地址映射、电源模式电流、时钟源、ADC/RF/外设规格、封装订购型号、PCB布局建议 |
| `docs/PHY622X ADC Application Note.md` | ADC / Voice | ADC引脚（P11/P14/P15/P18/P20/P23/P24/P25）、bypass vs attenuation模式、量程(0~0.8V/0~3.2V)、衰减系数校准、Voice AMIC/DMIC配置、`adc_Cfg_t` 结构体、`hal_voice_config()` |
| `docs/PHY622X GPIO Application Note.md` | GPIO / FULLMUX / KSCAN | 特殊IO限制(P16/P17/P1/SWD)、GPIO模式/retention/上下拉、中断唤醒配置、FULLMUX映射表(FMUX_UART0_TX=4等37个枚举)、KSCAN矩阵按键mk_in/mk_out与GPIO对应关系、完整API示例代码 |
| `docs/PHY622X Peripheral Application Note.md` | 外设驱动概述 | WATCHDOG(喂狗周期2~256s)、TIMER(用户可用TIM3~TIM6，时钟4MHz)、PWM(6路，16MHz源，UP/UP-DOWN模式占空比公式)、UART(2路，波特率计算公式，TX/RX FIFO=16B)、SPI(2路，FIFO=8，CPOL/CPHA)、I2C(2路，需外部上拉)、KSCAN(11行12列) |
| `docs/PHY622X_PWR_MGR应用指南.md` | 电源管理 | 4种功耗模式(正常/CPU休眠/深度休眠/standby/关机)、PWR_MGR API(`hal_pwrmgr_register/lock/unlock/RAM_retention/poweroff/enter_standby`)、MODULE_e枚举(MOD_USR0~8供应用使用)、功耗评估模型与计算公式、上电启动时间(冷启91ms/唤醒3.4ms) |
| `docs/PHY622X_Security_Boot_User_Guide.md` | 安全启动 | Efuse 4块(BLOCK0=ROM安全启动key，BLOCK1=OTA安全启动key)、efuse只能写一次且需奇校验、g_sec_key/g_ota_sec_key通过PhyPlusKit生成、ROM Security Boot流程、Flash地址映射(No OTA / Support OTA两种布局) |
| `docs/PHY62XX BSP_Button Application Note.md` | 按键中间件 | BSP_Button支持GPIO/KSCAN/混合三种硬件、按键类型(按下/释放/长按开始/长按保持)、组合按键配置、宏定义`BSP_BTN_HARDWARE_CONFIG/BSP_SINGLE_BTN_NUM/BSP_COMBINE_BTN_NUM`、`hal_gpio_btn_init()`/`hal_kscan_btn_check()`完整示例 |
| `docs/PHY62XX MESH 开发指南.md` | BLE Mesh开发 | EtherMind协议栈层次(7层)、工程目录结构、lib库说明(mesh_core/mesh_models/utils)、Model定义宏(USE_HSL/USE_CTL/USE_VENDORMODEL)、Mesh初始化完整流程(`appl_mesh_sample`)、Provision接口、Vendor Model/Generic OnOff回调实现示例、relay/proxy/friend开关API |

---

## 十二、常见宏定义

```c
// host_cfg.h 中定义BLE角色
HOST_CONFIG  // PERIPHERAL_CFG / CENTRAL_CFG / OBSERVER_CFG 按位组合

// 睡眠模式（编译宏）
CFG_SLEEP_MODE  // PWR_MODE_SLEEP 或 PWR_MODE_NO_SLEEP

// 芯片版本（封装）
SDK_VER_CHIP  // __DEF_CHIP_QFN32__（23IO）或 __DEF_CHIP_TSOP6252__（10IO）

// Flash保护
FLASH_PROTECT_FEATURE  // 0=关闭, 1=开启

// 动态时钟变频
CFG_HCLK_DYNAMIC_CHANGE  // 1=开启动态96M变频
```

