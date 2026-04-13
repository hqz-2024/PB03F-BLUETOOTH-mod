# PHY622X Peripheral Application Note

> v1.0 | phyplusinc | 2021.01.25 | 适用于：PHY6220 / PHY6250 / PHY6222 / PHY6252

> ⚠️ 以下所有外设：**系统休眠后配置丢失，唤醒后需重新初始化。**

---

## 1 WATCHDOG

| 属性 | 值 |
|------|----|
| 时钟源 | RC 32K 或 XTAL 32K（32.768kHz） |
| 喂狗周期 | 2s / 4s / 8s / 16s / 32s / 64s / 128s / 256s |
| 触发方式 | **轮询**：超时直接复位；**中断**：第一个周期产生中断，中断内可补喂，第二个周期未喂则复位 |
| 限制 | PHY6220/6250 不支持中断方式 |

---

## 2 TIMER

| 属性 | 值 |
|------|----|
| 硬件数量 | 6 个，其中 4 个被系统占用，**TIM3~TIM6 供应用使用** |
| 时钟源 | 固定 4MHz（驱动内软件 4 分频，等效 1MHz） |
| 位宽 | 32bit，最大计数值 0xFFFFFFFF |
| 模式 | free-running（减到0自动加载0xFFFFFFFF）/ user-defined（减到0加载用户配置值，驱动使用此模式） |
| 中断 | 支持；触发后自动重载继续计数，需手动关闭 |

---

## 3 PWM

| 属性 | 值 |
|------|----|
| 路数 | 6 路 |
| 时钟源 | 16MHz |
| 分频 | 1 / 2 / 4 / 8 / 16 / 32 / 64 / 128 |
| 引脚 | 所有支持 FULLMUX 的 IO 均可复用 |

### 占空比计算

| 模式 | POLARITY_FALLING | POLARITY_RISING |
|------|-----------------|-----------------|
| UP MODE | `(CMP_VAL+1) / (TOP_VAL+1)` | `1 - (CMP_VAL+1)/(TOP_VAL+1)` |
| UP AND DOWN MODE | `CMP_VAL / TOP_VAL` | `1 - CMP_VAL/TOP_VAL` |

### 频率计算（N = 分频系数）

| 模式 | 公式（MHz） |
|------|------------|
| UP MODE | `16 / N / (TOP_VAL + 1)` |
| UP AND DOWN MODE | `8 / N / TOP_VAL` |

**频率范围：**
- UP MODE：62.5kHz ~ 8MHz，占空比 0~100%（含边界）
- UP AND DOWN MODE：31.25kHz ~ 4MHz，占空比不含 0% 和 100%

---

## 4 UART

| 属性 | 值 |
|------|----|
| 路数 | 2 路（UART0 / UART1） |
| TX/RX FIFO | 各 16 字节 |
| 时钟 | PCLK = HCLK（不建议分频） |
| 默认日志口 | UART0（P9=TX，P10=RX），由 `DEBUG_INFO` 宏控制 |
| 引脚 | 所有支持 FULLMUX 的 IO 均可复用 |

**波特率计算：**

```
divisor = HCLK / (16 × baud_rate)
```

误差 > 2% 时该波特率不可用（如 48MHz 下不支持 921600，支持 115200 和 1000000）。

帧格式：`[1 start] [5~9 data bits] [0~1 parity] [1~2 stop]`

---

## 5 SPI

| 属性 | 值 |
|------|----|
| 路数 | 2 路，可配 MASTER / SLAVE |
| FIFO | TX/RX 各 8，数据位宽 4~16bit 可配 |
| 时钟 | HCLK（不建议分频） |
| CS 控制 | `force_cs=TRUE` 手动（GPIO 控制），`FALSE` 自动 |
| 中断 | `int_mode=TRUE` 启用 |

**时钟速率限制：**

| 角色 | 约束 |
|------|------|
| Master | `PCLK ≥ 2 × F_sclk_out` |
| Slave（仅收） | `PCLK ≥ 6 × F_sclk_in` |
| Slave（收发） | `PCLK ≥ 8 × F_sclk_in` |

**CPOL / CPHA 模式：**

| CPOL | CPHA | SCK 空闲 | 采样沿 |
|------|------|---------|-------|
| 0 | 0 | 低 | 前沿（上升沿） |
| 0 | 1 | 低 | 后沿（下降沿） |
| 1 | 0 | 高 | 前沿（下降沿） |
| 1 | 1 | 高 | 后沿（上升沿） |

---

## 6 I2C

| 属性 | 值 |
|------|----|
| 路数 | 2 路，可配 MASTER / SLAVE |
| FIFO | TX/RX 各 8 字节 |
| 时钟 | PCLK = HCLK（不建议分频） |
| 上拉 | **必须外接上拉电阻**（推荐 2.2kΩ 或 4.7kΩ） |
| 引脚 | 所有支持 FULLMUX 的 IO 均可复用 |

---

## 7 KSCAN

| 属性 | 值 |
|------|----|
| 最大规模 | 11 行 × 12 列 |
| 引脚限制 | 不建议使用 P16、P17 作列 |
| 引脚映射 | 见 `kscan.h`（mk_in[] = 行，mk_out[] = 列） |
| 触发 | 按键按下时产生中断，回调上报 row/col |

