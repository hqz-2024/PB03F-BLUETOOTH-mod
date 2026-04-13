# PHY6222 BLE 5.2 SoC 数据手册（v1.4）

## 核心特性

- **主控**：ARM® Cortex™-M0 32 位处理器，最高 96MHz，支持 SWD 调试
- **内存**：Flash 128KB~8MB 片内可编程；SRAM 64KB 睡眠全 retention；ROM 96KB；eFuse 256bit；指令缓存 4 路 8KB
- **GPIO**：22 个通用 IO，睡眠 / 掉电状态保持，支持唤醒 / 中断
- **外设**：3 路 QDEC、6 路 PWM、2 路 I2C/SPI/UART、4 路 DMA、内置 DMIC/AMIC、10 通道 12bit ADC + 低噪声语音 PGA、6 路 32bit 定时器、1 路看门狗、RTC
- **电源**：1.8V~3.6V，内置 BUCK DC-DC+LDO；掉电 0.3μA（IO唤醒）；睡眠 1μA（RTC）；RX 4mA；TX 4.6mA@0dBm；MCU <60μA/MHz
- **时钟**：内置 16MHz/32MHz RC，支持 DLL 倍频至 96MHz；32KHz RTC 精度 ±500ppm
- **BLE**：兼容 BLE 5.2，支持 2Mbps/DLE，吞吐量最高 1.6Mbps，支持 AoA/AoD、SIG-Mesh
- **灵敏度**：-105dBm@125Kbps，-99dBm@1Mbps，-96dBm@2Mbps
- **发射功率**：-20dBm~+10dBm（3dB 步进）
- **安全**：硬件 AES-128，随机数发生器
- **温度**：消费级 -40℃~+85℃；工业级 -40℃~+105℃
- **封装**：QFN32 (4mm×4mm)、QFN24 (3mm×3mm)，RoHS 合规
- **应用**：可穿戴、信标、智能家居、医疗、工业 IoT、数据传输

---

**版本历史**：1.0(2020.11) → 1.1(2020.12) → 1.2(2021.04) → 1.3(2021.06) → 1.4(2023.03)

---

## 1 产品概述

PHY6222 是面向 BLE 5.2 应用的片上系统，集成 Cortex-M0 内核、低功耗射频、完整外设与电源管理，支持 OTA 升级，最小化 BOM 成本。

### 1.2 引脚分配

#### 1.2.1 PHY6222QC (QFN32) 引脚定义

| 引脚 | 引脚名 | 功能描述 |
|------|--------|---------|
| 1 | P0 | GPIO 0 |
| 2 | P1 | GPIO 1 |
| 3 | P2/SWD_IO | GPIO 2 / SWD 调试数据 |
| 4 | P3/SWD_CLK | GPIO 3 / SWD 调试时钟 |
| 5 | VDDDEC | 1.2V 去耦引脚 |
| 6 | P7 | GPIO 7 |
| 7 | TM | 测试模式使能 |
| 8 | P9 | GPIO 9 |
| 9 | P10 | GPIO 10 |
| 10 | P11/AIO_0 | GPIO 11 / ADC 输入 0 |
| 11 | P14/AIO_3 | GPIO 14 / ADC 输入 3 |
| 12 | VDD_LDO | 内部 LDO 电源 / DC-DC 反馈 |
| 13 | DCDC_SW | DC-DC 输出 |
| 14 | VDD3 | 3.3V 电源 |
| 15 | P15/AIO_4 | GPIO 15 / ADC 输入 4 / 麦克风偏置 |
| 16 | XTAL16M_I | 16MHz 晶振输入 |
| 17 | XTAL16M_O | 16MHz 晶振输出 |
| 18 | P16/XTAL32K_I | GPIO16 / 32.768KHz 晶振输入 |
| 19 | P17/XTAL32K_O | GPIO17 / 32.768KHz 晶振输出 |
| 20 | P18/AIO_7 | GPIO 18 / ADC 输入 7 / PGA 负输入 |
| 21 | P20/AIO_9 | GPIO 20 / ADC 输入 9 / PGA 正输入 |
| 22 | RST_N | 复位（低有效） |
| 23 | VDD_RF | 射频电源去耦 |
| 24 | RF | 射频天线 |
| 25 | P23/AIO_1 | GPIO 23 / ADC 输入 1 / 麦克风偏置参考 |
| 26 | P24/AIO_2 | GPIO 24 / ADC 输入 2 |
| 27 | P25/AIO_8 | GPIO 25 / ADC 输入 8 |
| 28 | P26 | GPIO 26 |
| 29 | P31 | GPIO 31 |
| 30 | P32 | GPIO 32 |
| 31 | P33 | GPIO 33 |
| 32 | P34 | GPIO 34 |

> 所有 GPIO 支持 1MΩ/150kΩ 上拉、150kΩ 下拉。

#### 1.2.2 PHY6222QH (QFN24) 引脚定义

| 引脚 | 引脚名 | 功能描述 |
|------|--------|---------|
| 1 | P2 | GPIO 2 / SWD_IO |
| 2 | P3 | GPIO 3 / SWD_CLK |
| 3 | P9 | GPIO 9 |
| 4 | P10 | GPIO 10 |
| 5 | P11 | GPIO 11 / ADC 输入 0 |
| 6 | P14 | GPIO 14 / ADC 输入 3 |
| 7 | VDD_LDO | 内部 LDO 电源 / DC-DC 反馈 |
| 8 | DCDC_SW | DC-DC 输出 |
| 9 | VDD3 | 3.3V 电源 |
| 10 | P15 | GPIO 15 / ADC 输入 4 / 麦克风偏置 |
| 11 | XTAL16M_I | 16MHz 晶振输入 |
| 12 | XTAL16M_O | 16MHz 晶振输出 |
| 13 | P16 | GPIO16 / 32.768KHz 晶振输入 |
| 14 | P17 | GPIO17 / 32.768KHz 晶振输出 |
| 15 | P18 | GPIO 18 / ADC 输入 7 / PGA 负输入 |
| 16 | RST_N | 复位（低有效） |
| 17 | VDD_RF | 射频电源去耦 |
| 18 | RF | 射频天线 |
| 19 | P23 | GPIO 23 / ADC 输入 1 / 麦克风偏置参考 |
| 20 | P24 | GPIO 24 / ADC 输入 2 |
| 21 | P33 | GPIO 33 |
| 22 | P34 | GPIO 34 |
| 23 | P0 | GPIO 0 |
| 24 | P1 | GPIO 1 |

---

## 2 系统模块

### 2.1 CPU

- **内核**：ARM Cortex-M0，最高 96MHz
- **架构**：ARMv6M，Thumb 指令集，3 级流水线
- **中断**：32 个中断，嵌套向量中断控制器 (NVIC)
- **调试**：SWD 串行线调试，无 TRACE

### 2.2 存储器

| 内存类型 | 容量 | 功能 |
|---------|------|------|
| ROM | 96KB | BootLoader、协议栈、外设驱动 |
| SRAM0 | 32KB | 程序 / 数据存储，睡眠保持（0x1fff0000~0x1fff7fff） |
| SRAM1 | 16KB | 程序 / 数据存储，睡眠保持（0x1fff8000~0x1fffbfff） |
| SRAM2 | 16KB | 程序 / 数据存储，睡眠保持（0x1fffc000~0x1fffffff） |
| SRAM_BB | 4KB | 协议栈缓存 |
| SRAM_cache | 8KB | 4路指令缓存 |
| Flash | 128KB~8MB | 可编程片内 Flash（XIP，基地址 0x11000000） |
| eFuse | 256bit | 一次性可编程非易失存储 |

### 2.3 启动模式

上电后进入 CP 启动模式，ROM 映射到 0x0 地址；BootLoader 校验 Flash 有效性，有效则正常启动，无效则进入 Flash 编程模式。

### 2.4 电源、时钟与复位 (PCR)

#### 2.4.1 电源管理

- 工作模式：正常、时钟门控、睡眠、掉电
- 内置 BUCK DC-DC+LDO，支持电池监测
- **UVLO 欠压锁定**：VDD > 1.78V 释放复位，< 1.69V 进入复位

| 模式 | 电流 | 条件 |
|------|------|------|
| 掉电模式 | 0.3μA | 仅 IO 唤醒 |
| 睡眠模式 | 1μA | 32KHz RTC |
| 睡眠模式 | 13μA | 32KHz RTC + 全 SRAM 保持 |
| 接收模式 | 4mA | 3.3V |
| 发射模式 | 4.6mA | 3.3V，0dBm |

#### 2.4.3 时钟源

- 晶振：16MHz（必需）、32.768KHz（可选，否则使用内置RC32K）
- 内置 RC：32MHz（精度 3%）、32KHz（精度 ±500ppm）
- **支持 DLL 倍频**：32 / 48 / 64 / 96MHz（`SYS_CLK_DBL_32M` / `SYS_CLK_DLL_48M` / `SYS_CLK_DLL_64M` / `SYS_CLK_DLL_96M`）

### 2.5 IOMUX 与 GPIO

- **IOMUX**：外设引脚灵活映射，支持 I2C/UART/SPI/PWM/QDEC 重映射至任意 GPIO
- **GPIO**：22 个通用 IO，支持中断、唤醒、防抖

**电气特性**（25℃，VDD=3V）：
- 输入低电压：≤ 0.5V；输入高电压：≥ 2.4V
- 输入漏电流：±50nA；输出驱动：10mA

---

## 3 外设模块

### 3.1 2.4GHz 射频收发器

- **频段**：2.4~2.4835GHz ISM 频段
- **速率**：125Kbps / 500Kbps / 1Mbps / 2Mbps
- **灵敏度**：-105dBm@125Kbps，-99dBm@1Mbps，-96dBm@2Mbps
- **发射功率**：-20dBm~+10dBm（3dB 步进）
- 集成巴伦，单引脚天线，无需 RX/TX 切换引脚

### 3.2 定时器与 RTC

- 6 路 32bit 通用定时器（TIM1~TIM6）、1 路看门狗
- RTC：24 位计数器，低功耗实时计时
- **注**：TIM1/TIM2 被 BLE 协议栈占用，用户可用 TIM3~TIM6

### 3.3 通信接口

- **SPI**：2 路独立（SPI0/SPI1），支持主 / 从模式，支持 DMA
- **I2C**：2 路独立（I2C_0/I2C_1），支持 100KHz / 400KHz
- **UART**：2 路独立（UART0/UART1），最高 1Mbps，支持硬件流控 / DMA

### 3.4 音频与 ADC

- **音频**：支持 AMIC / DMIC，采样率 8K/16K/32K/64K，CVSD/PCM 压缩
- **ADC**：12bit SAR，10 路输入，PGA 增益 0~42dB（3dB 步进）
- 支持单端 / 差分采样，自动通道扫描

### 3.5 其他外设

- **PWM**：6 路独立输出（PWM0~PWM5），可编程占空比 / 频率
- **QDEC**：3 路正交解码，支持机械 / 光学传感器
- **按键扫描（KSCAN）**：最大 16×18 矩阵，支持自动 / 手动模式
- **加密**：硬件 AES-128，随机数发生器 (RNG)

---

## 4 电气参数

### 4.1 绝对最大额定值

| 参数 | 最小值 | 最大值 | 单位 |
|------|--------|--------|------|
| VDD3 电源电压 | -0.3 | +3.6 | V |
| IO 引脚电压 | -0.3 | VDD+0.3 | V |
| 存储温度 | -40 | +125 | ℃ |
| 湿度敏感等级 (MSL) | — | 3 | — |
| ESD (HBM) | — | 2 | kV |
| Flash 擦写寿命 | — | 100000 | 次 |

### 4.2 工作条件

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|------|--------|--------|--------|------|
| 工作电压 | 1.8 | 3.0 | 3.6 | V |
| 消费级温度 | -40 | 27 | 85 | ℃ |
| 工业级温度 | -40 | 27 | 105 | ℃ |

---

## 5 封装与订购信息

### 5.1 封装尺寸

- **QFN32**：4mm×4mm，厚度 0.7~0.8mm（23 个 GPIO）
- **QFN24**：3mm×3mm，厚度 0.5~0.6mm（少量 GPIO）

### 5.2 订购型号

| 型号 | 封装 | 电压 | 温度 | Flash | 状态 |
|------|------|------|------|-------|------|
| PHY6222QC-W041 | QFN32 | 1.8~3.6V | -40~105℃ | 512KB | 量产 |
| PHY6222QC-W04C | QFN32 | 1.8~3.6V | -40~85℃ | 512KB | 量产 |
| PHY6222QC-W16C | QFN32 | 1.8~3.6V | -40~85℃ | 2MB | 量产 |
| PHY6222QH-W02C | QFN24 | 1.8~3.6V | -40~85℃ | 256KB | 量产 |

---

## 6 应用电路与布局指南

### 6.1 典型应用

- 支持带 DC-DC / 不带 DC-DC 两种电源架构
- 射频输出功率 > 5dBm 时，DCDC 滤波电容 C3 = 4.7μF

### 6.2 PCB 布局建议

- **射频**：RF 走线 50Ω 阻抗控制，表层布线，第二层完整地平面
- **电源**：VDD 引脚就近加旁路电容，电源走线短且粗
- **晶振**：走线尽量短，远离射频走线和干扰源，周围加接地过孔
- **接地**：大量接地过孔，旁路电容地端就近打孔接地
- **PCB叠层**：推荐 4 层板；双层板需最大化地平面并屏蔽关键信号

---

## 7 术语表

| 缩写 | 全称 |
|------|------|
| BLE | Bluetooth Low Energy |
| SoC | System on Chip |
| MCU | Micro Controller Unit |
| RTC | Real Time Counter |
| ADC | Analog to Digital Converter |
| PGA | Programmable Gain Amplifier |
| QDEC | Quadrature Decoder |
| PWM | Pulse Width Modulation |
| SWD | Serial Wire Debug |
| DLL | Delay-Locked Loop（锁相环倍频） |
| UVLO | Under Voltage Lock Out（欠压锁定） |
| XIP | Execute In Place（Flash原地执行） |
| AMIC | Analog Microphone |
| DMIC | Digital Microphone |