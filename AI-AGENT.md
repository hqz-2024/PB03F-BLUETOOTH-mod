# AI 协作说明

> 本文档由工具根据仓库代码扫描归纳生成，可人工修订。

---

## 1. 项目概要

- **项目名称**：PHY62XX BLE SoC 固件（SDK 版本 `PHY62XX_SDK_3.1.5`）
- **目标芯片**：PHY6222 / PHY62XX 系列（Cortex‑M0 内核），支持 QFN32、TSOP16 等封装。
- **主要功能**：BLE 协议栈（Controller + Host）、BLE Mesh（Ethermind/Mindtree）、多种外设驱动、OTA 固件更新、文件系统、密码学库、测试/演示应用。
- **编程语言**：C（部分头文件包裹 `extern "C"` 以兼容 C++ 编译器）。

---

## 2. 技术栈与依赖

| 类别 | 技术/库 |
|------|---------|
| **内核** | ARM Cortex‑M0（CMSIS V3.20） |
| **BLE 协议栈** | BLE Controller（LL、RF PHY）、Host（GAP、GATT、ATT、SM、L2CAP） |
| **BLE Mesh** | Mindtree EtherMind Mesh 栈（API 前缀 `MS_`、`EM_`） |
| **操作系统抽象** | OSAL（任务、消息、定时器、内存管理），部分模块封装了 FreeRTOS 定时器接口 |
| **传输层** | HCI UART（参数定义在 `hci_task.h`） |
| **密码学** | TinyCrypt 0.2.8（AES‑128/CCM/CMAC、HMAC、SHA‑256、ECC P‑256）；mbed TLS 风格 SHA‑256；自定义配对加密函数（`f4`/`f5`/`g2` 等） |
| **文件系统** | 自定义 Flash 文件系统（磨损均衡、垃圾回收、可选 RAID1） |
| **硬件抽象** | 外设驱动以 `hal_` 前缀封装（GPIO、I2C、SPI、UART、PWM、ADC、DMA、Flash 等） |
| **标准库** | `<stdint.h>`, `<stdio.h>`, `<string.h>`, `<stdlib.h>`, `<math.h>` 等 |
| **编译器** | GCC（主要）、IAR、Keil（`__CC_ARM`/`__ICCARM__`/`__GNUC__` 分支适配） |

---

## 3. 目录与模块组织

项目根目录：`release_bbb_sdk-PHY62XX_SDK_3.1.5/`

### 3.1 BLE 协议栈
- **Controller**：`components/ble/controller/`  
  `ll.h`, `ll_def.h`, `ll_common.h`, `ll_enc.h`, `ll_hw_drv.h`, `ll_sleep.h`, `ll_debug.h`，以及 RF 驱动 `rf_phy_driver.h`
- **HCI**：`components/ble/hci/`  
  `hci_task.h`, `hci_event.h`, `hci_data.h`, `hci_tl.h`, `hci_host.h`
- **Host**：`components/ble/host/`  
  `gap.h`, `gatt_internal.h`, `sm_internal.h`, `l2cap_internal.h`, `att_internal.h` 等
- **公共 API**：`components/ble/include/`  
  `bcomdef.h`, `att.h`, `gap.h`, `sm.h`, `hci.h` 等

### 3.2 外设驱动
- `components/driver/` 按外设分子目录：`adc/`, `clock/`, `dma/`, `flash/`, `gpio/`, `i2c/`, `pwm/`, `timer/`, `uart/` 等
- 每个目录包含 `.c`‑`.h` 对；部分模块有变体（如 `i2c.c` 与 `i2c_io.c` 软件模拟版本）
- 板级支持：`bsp_button/` 提供按键驱动

### 3.3 BLE Mesh（Ethermind）
- **协议栈核心**：`components/ethermind/mesh/export/include/`  
  `MS_access_api.h`, `MS_config_api.h`, `MS_net_api.h`, `MS_prov_api.h` 及各模型 API 头文件
- **应用模型**：`components/ethermind/mesh/export/appl/model/`  
  `client/` 和 `server/` 子目录（Generic OnOff、Light、Sensor、Scene 等），每个模型一个 `.c`‑`.h`
- **CLI 工具**：`components/ethermind/mesh/export/cli/`，包含 `cli_core.c`、模型客户端/服务器 CLI 文件
- **平台适配**：`components/ethermind/platforms/`（`EM_platform.c`, `blebrr_pl.c`, `mesh_services.c` 等）
- **OS 抽象**：`components/ethermind/osal/src/phyos/`（`EM_os.c`, `EM_timer.c`, `EM_debug.c`）

### 3.4 组件库与 Profile
- **通用库**：`components/libraries/`  
  `circular_buf/`, `cli/`（命令行接口及 AT 命令）, `crc16/`, `crypto/`（AES‑128/ECDH/SHA‑256）, `datetime/`, `fs/`（文件系统）, `secure/`, `tinycrypt-0.2.8/`, `xmodem/`
- **GATT Profiles**：`components/profiles/`  
  `HID/`, `HIDVoice/`, `Keys/`, `Roles/`（`peripheral.c`, `central.c`, `broadcaster.c` 等）, `ota/`, `aliGenie/`, `ancs/`, `ScanParam/`, `hrs/`（心率）, `ppsp/` 等

### 3.5 示例与测试
- `example/OTA/`：多个 OTA 例程（`OTA_internal_flash`, `otadongle`, `slboot`）
- `components/ethermind/mesh/export/sample/`：`appl_sample_example_*.c` 演示 Mesh 角色和模型
- `components/coremark/`：CoreMark 基准测试代码
- `components/libraries/fs/`：包含 `fs_autotest.c`, `fs_test.c` 文件系统自动化测试

### 3.6 平台与启动
- `components/arch/cm0/`：Cortex‑M0 核心配置
- `components/inc/`：`mcu.h`, `bus_dev.h`, `error.h`, `global_config.h`, `types.h` 等
- `components/gcc/CMSIS/`：ARM CMSIS 头文件（`core_cm0.h`, `core_cmFunc.h` 等）及启动文件（`phy6222_cstart.c`, `phy6222_vectors.c`）

---

## 4. 代码风格

### 4.1 命名约定
- **宏 / 常量**：全大写 + 下划线（`LL_STATUS_SUCCESS`, `MAX_NUM_LL_CONN_ROM_LIMT`, `PPlus_SUCCESS`）
- **类型定义**：结构体/枚举/联合体使用 `_t` / `_e` 后缀（`llConnState_t`, `bStatus_t`, `gpio_pin_e`, `spi_Cfg_t`）
- **函数**：
  - BLE 协议栈：模块前缀 + 动词 + 名词（`LL_Init`, `GAP_DeviceInit`, `HCI_Reset`, `ATT_ExchangeMTUReq`）
  - 驱动层：`hal_<模块>_<动作>`（`hal_adc_init`, `hal_gpio_write`）
  - Mesh 模型：`MS_<模块>_<动作>` / `appl_<模型>_<动作>`（`MS_health_server_init`, `appl_send_generic_onoff_set`）
  - 应用层 / Profile：`HidDev_Init`, `GAPRole_StartDevice`, `AudioProfile_AddService`
- **变量**：
  - 全局变量：`g_` 前缀（`g_ll_conn_ctx`, `g_ota_status`）
  - 静态局部/模块变量：`s_` 或 `m_` 前缀（`s_ota_app`, `m_kscanCtx`）
  - 局部变量：小写 + 下划线（`conn_ptr`, `p_adv_info`）
- **文件名**：小写 + 下划线（`ll_def.h`, `gap_internal.h`, `rf_phy_driver.h`），`.c`/`.h` 成对

### 4.2 注释
- **文件头**：所有源文件包含 Phyplus 保密声明，以及 Doxygen 风格的 `\file`, `\brief`, 部分有 `\version`, `\date`, `\author`
- **函数文档**：对外 API 使用 Doxygen 块注释（`@fn`, `@brief`, `@param`, `@return`）；参数方向标记 `/* IN */` / `/* OUT */` / `/* INOUT */`
- **行内注释**：常用 `/* ... */` 或 `//`，关键参数或寄存器位域使用 `//!<` 后缀说明
- **分隔符**：`/* ------------------------------------ */` 用于分割代码节（INCLUDES、MACROS、CONSTANTS 等）
- **特殊标记**：
  - `/* TODO: */` 标记未完成事项
  - `// 2020-01-23 add error Code` 等日期注释
  - `#if 0 … #endif` 包含未启用代码块，常用于保留备用逻辑或测试代码

### 4.3 格式与编译
- **缩进**：4 空格（少数地方 8 空格），统一使用空格
- **大括号**：函数体大括号另起一行；`if`/`for`/`switch` 左大括号紧跟语句
- **条件编译**：多重嵌套，支持 `#ifdef`, `#if`, `#if defined(...)`，常见模块开关和编译器分支
- **头文件保护**：`#ifndef _H_FILENAME_H_` / `#define _H_FILENAME_H_`
- **代码段属性**：使用 `__ATTR_SECTION_SRAM__`, `__ATTR_SECTION_XIP__` 指定内存布局；中断函数标记 `__attribute__((used))` 以防止被优化

---

## 5. 错误处理

- **统一状态码**：
  - BLE/Profile 返回 `bStatus_t`（`SUCCESS`, `INVALIDPARAMETER`, `bleNotConnected` 等）
  - 驱动/系统返回 `PPlus_SUCCESS`, `PPlus_ERR_INVALID_PARAM`, `PPlus_ERR_BUSY` 等（定义于 `error.h`）
  - Mesh 栈返回 `API_RESULT`（`API_SUCCESS` / `API_FAILURE`，内部扩展更多错误码）
- **检查模式**：函数入口进行参数校验（NULL 指针、长度、范围），失败立即返回错误码；超时处理使用 `xxx_CHECK_TOUT` 宏
- **回调**：回调函数通常携带 `status` 参数，应用需要检查状态后再进行下一步
- **无异常**：纯 C 语言，不使用 C++ 异常机制

---

## 6. 日志与调试

### 6.1 日志宏
项目使用多套日志系统，根据模块和编译配置选择：
- **通用**：`LOG(...)`（`printf` 风格，通过串口 `P9/P10` 输出，常用于 Profile 和驱动）
- **BLE Mesh 应用层**：`CONSOLE_OUT(...)`（定义为 `printf`），使用 `[MODULE]` 标签和 `[ERR]` 前缀
- **AT 命令**：`AT_LOG(...)`
- **文件系统**：`FS_LOG(...)`，由 `FS_DBBUG` 控制
- **Ethermind 协议栈内部**：分级宏 `ACCESS_ERR`, `EM_debug_error`, `EM_debug_trace` 等，可通过模块级 `*_NO_DEBUG` 或 `DEBUG_INFO` 宏开关
- **驱动调试**：`LOG_DEBUG(...)`，由 `DEBUG_INFO` 宏控制输出

### 6.2 日志特征
- 十六进制打印：`my_dump_byte()`, `appl_dump_bytes()`, `LOG_DUMP_BYTE`
- 错误输出常包含函数名、错误码（如 `“[ERR] init failed: 0x%04X”`）
- 部分调试信息通过条件编译禁用（`#if DEBUG_INFO > 1`）

---

## 7. 测试与质量保障

### 7.1 现有测试
- **CoreMark**：`components/coremark/` 提供 EEMBC CoreMark 基准测试，验证 CPU 性能并输出 CRC 交叉校验
- **TinyCrypt**：`components/libraries/tinycrypt-0.2.8/tests/` 内有独立测试程序，使用 NIST/RFC 标准向量，覆盖正常/边界/错误注入等场景，`TC_START`/`TC_END_RESULT` 宏驱动
- **文件系统**：`components/libraries/fs/` 的 `fs_autotest.c`/`fs_test.c` 提供 CLI 命令，进行读写、垃圾回收、CRC 校验、掉电模拟等测试
- **射频测试**：`rf_phy_driver.h` 暴露直接测试模式（DTM）函数，用于认证和产测
- **GATT 一致性测试**：`gatttest.h` 提供 GATT 资格测试服务
- **Mesh 交互测试**：Mesh 模型带控制台菜单（`main_xxx_client_operations`），可手动发送 Get/Set 命令并检查返回状态

### 7.2 缺失
- **尚未发现** 统一的单元测试框架（如 Unity/CUnit）或自动化回归测试脚本。
- 多数功能验证依赖手动 CLI 操作或示例程序，测试覆盖不完全。

---

## 8. 配置与环境

项目**不使用 Kconfig 或外部配置文件**，所有配置通过**预编译宏**完成，分散在各模块头文件中。

### 8.1 常用配置宏
- **芯片型号**：`__DEF_CHIP_QFN32__` / `__DEF_CHIP_TSOP16__`，影响 RF 功率等
- **连接限制**：`MAX_NUM_LL_CONN_ROM_LIMT`, `MAX_CONNECTION_MASTER_NUM`, `MAX_CONNECTION_SLAVE_NUM`
- **功能开关**：`EN_CONSUMER_MODE`, `EXT_ADV_ENABLE`, `BLE_AT_ENABLE`, `USE_FCT`, `CFG_OTA_BANK_MODE`
- **电源管理**：`CFG_SLEEP_MODE`（`PWR_MODE_NO_SLEEP` / `SLEEP` / `PWROFF_NO_SLEEP`）
- **Mesh 特性**：`MS_PROXY_SUPPORT`, `MS_RELAY_SUPPORT`, `MS_LPN_SUPPORT`, `MS_HAVE_DYNAMIC_CONFIG`
- **容量限制**：`MS_CONFIG_LIMITS(MS_ACCESS_ELEMENT_COUNT)` 等，通过 `MS_limits.h` 或 `MS_limit_config.c` 设置
- **存储**：`MS_STORAGE` 使能持久化配置存储，`USE_FS` 决定 SNV 使用文件系统还是直接 Flash 操作

### 8.2 硬件地址与布局
- 寄存器操作使用基址 + 偏移宏（`LL_HW_BASE`, `AP_UART0_BASE`），通过 `*(volatile uint32_t *)` 访问
- Flash 分区地址硬编码（如 `OTA_MODE_SELECT_REG 0x4000f034`, `NVM_BASE_ADDR`）
- 部分代码使用 `#if` 检查配置合法性（`#error` 指令）

---

## 9. 硬件抽象与寄存器操作

- 驱动层已封装函数（`subWriteReg()`, `read_reg()`, `BIT(n)` 等），**应优先使用这些接口**，避免直接写 `volatile` 裸指针。
- 中断处理：通过跳转函数表 `JUMP_FUNCTION(IRQ_HANDLER)` 动态注册，配合 `NVIC_SetPriority` 和 `NVIC_EnableIRQ`；中断服务程序需用 `__attribute__((used))` 防止优化。
- 电源管理：外设驱动需调用 `hal_pwrmgr_register` 注册睡眠/唤醒回调，并使用 `hal_pwrmgr_lock/unlock` 控制睡眠条件。
- 内存布局：关键代码/数据通过 `__ATTR_SECTION_SRAM__`（运行于 SRAM）或 `__ATTR_SECTION_XIP__`（片上 eXecute In Place）放置。

---

## 10. 对 AI 编码助手的约束

### 10.1 代码生成原则
1. **遵循现有命名和风格**：新模块/函数必须沿用本文档描述的命名规则（如 `hal_<periph>_<action>`, `MS_<module>_<action>`, `appl_<model>_<action>`），并写 Doxygen 注释。
2. **错误返回统一**：函数必须返回合适的错误码（`API_RESULT`, `bStatus_t`, `PPlus_SUCCESS` 等），不允许简单忽略错误。
3. **内存管理**：动态内存必须使用平台提供的 `osal_mem_alloc`/`osal_mem_free` 或 `EM_alloc_mem`/`EM_free_mem`，禁止直接调用标准 `malloc`/`free`。
4. **日志输出**：使用当前模块对应的日志宏（`LOG()`, `CONSOLE_OUT()`, `FS_LOG()` 等），严禁直接使用 `printf`（除非模块已明确定义为 `printf`）。
5. **中断与电源管理**：中断服务程序必须通过跳转表注册，且标记 `__attribute__((used))`；电源管理相关的模块须提供睡眠/唤醒回调并调用 `hal_pwrmgr_register`。
6. **配置开关**：新增功能或调整资源占用，应通过宏配置，遵循 `#ifndef` + `#define` 模式，并保持与其他宏命名风格一致。
7. **Mesh 模型开发**：新增模型需遵循固定模板（初始化 → 回调注册 → 状态 get/set），结构体/回调签名需与 `MS_` API 完全匹配；CLI 命令沿用 `cli_modelc_<model>_<action>` 的命名方式。

### 10.2 模块组织
- 新外设驱动放在 `components/driver/<periph>/` 下，提供 `hal_<periph>_init` / `deinit` 等标准接口，并定义 `xxx_Cfg_t` 和 `xxx_Evt_t`。
- 新 GATT Profile 放在 `components/profiles/<Profile>/`，实现 Service/Characteristic 属性表及 Read/Write 回调。
- 文件命名统一小写 + 下划线，头文件保护宏使用 `_H_<模块名>_` 模式。

### 10.3 版权与保密
> **注意**：所有源代码头部均包含 **Phyplus 机密与专有信息声明**，指出代码不得被修改、复制、分发或泄露。AI 辅助开发时应避免将完整代码片段粘贴到公开环境，并遵守相关保密约定。本文档仅归纳事实，不表示授权。

---

## 11. 待确认事项
- Mesh 模型服务器状态处理中，部分 `TODO` 和 `Hack` 标记可能指示未完成的实现（如多元素 instance 1 初始化、内存分配失败检查缺失等），继续开发前需人工复核。
- 文件系统与 `fs2/` 目录关系未明，可能为不同版本实现，后续统一需对照产品需求。
- 部分旧注释指向前期调试工具（如 `HID_TEST_EVT`），其测试逻辑是否仍有效、需保留待与团队确认。

---

## 12. 禁止事项
- **不要编造仓库中不存在的 API、路径、配置宏、寄存器地址或函数签名。** 生成代码前务必在仓库中搜索确认。
- **不要擅自删除或改写** 文件头部的保密声明和版权注释。
- **不要使用 `printf`、`malloc`、`free`** 等未在该项目中封装的 C 标准库函数，除非对应模块已显式包含其映射（如 `CONSOLE_OUT` 定义为 `printf`）。
- **不要引入额外的构建系统配置文件**（如 Kconfig、CMakeLists.txt），项目完全依赖预编译宏和 IDE 工程配置。
- **不要改变现有任务/事件架构或中断向量表注册方式**，新增模块必须沿用 OSAL 事件模型和跳转表动态注册。