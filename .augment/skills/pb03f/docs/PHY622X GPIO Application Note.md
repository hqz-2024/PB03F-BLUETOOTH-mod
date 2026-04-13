# PHY622X GPIO Application Note

> v1.0 | phyplusinc | 2021.01.25 | 适用于：PHY6220 / PHY6250 / PHY6222 / PHY6252

---

## 1 引脚默认属性（QFN32）

| 引脚 | 默认模式 | 方向 | 唤醒 | IRQ | FULLMUX | ANA | KSCAN |
|------|---------|------|------|-----|---------|-----|-------|
| P00 | GPIO | IN | ✓ | ✓ | ✓ | | mk_in[0] |
| P01 | GPIO | IN | ✓ | ✓ | | | mk_out[0] |
| P02 | SWD_IO | OUT | ✓ | ✓ | ✓ | | mk_in[1] |
| P03 | SWD_CLK | IN | ✓ | ✓ | ✓ | | mk_out[1] |
| P07 | GPIO | IN | ✓ | ✓ | ✓ | | mk_in[10] |
| P09 | GPIO | IN | ✓ | ✓ | ✓ | | mk_out[4] |
| P10 | GPIO | IN | ✓ | ✓ | ✓ | | mk_in[4] |
| P11 | GPIO | IN | ✓ | ✓ | ✓ | ✓ | mk_out[11] |
| P14 | GPIO | IN | ✓ | ✓ | ✓ | ✓ | mk_out[2] |
| P15 | GPIO | IN | ✓ | ✓ | ✓ | ✓ | mk_in[2] |
| P16 | XTALI | ANA | | | | ✓ | mk_out[10] |
| P17 | XTALO | ANA | | | | ✓ | mk_out[9] |
| P18 | GPIO | IN | ✓ | | ✓ | ✓ | mk_in[5] |
| P20 | GPIO | IN | ✓ | | ✓ | ✓ | mk_out[5] |
| P23 | GPIO | IN | ✓ | | ✓ | ✓ | mk_in[6] |
| P24 | GPIO | IN | ✓ | | ✓ | ✓ | mk_out[3] |
| P25 | GPIO | IN | ✓ | | ✓ | ✓ | mk_in[3] |
| P26 | GPIO | IN | ✓ | | ✓ | | mk_out[8] |
| P27 | GPIO | IN | ✓ | | ✓ | | mk_in[9] |
| P31 | GPIO | IN | ✓ | | ✓ | | mk_out[7] |
| P32 | GPIO | IN | ✓ | | ✓ | | mk_in[7] |
| P33 | GPIO | IN | ✓ | | ✓ | | mk_out[6] |
| P34 | GPIO | IN | ✓ | | ✓ | | mk_in[8] |

---

## 2 特殊引脚限制

### TEST_MODE

与 P24、P25 共同决定芯片启动模式，应用代码不能使用 TEST_MODE：

| TEST_MODE | P24 | P25 | 模式 |
|-----------|-----|-----|------|
| 0 | * | * | Normal / Program Mode（通过握手切换） |
| 1 | 0 | 0 | Program Mode（上电直接进入） |
| 1 | 0 | 1 | Scan Mode |
| 1 | 1 | 0 | Bist Mode |

### P16、P17

接 32.768K 晶振和电容组成振荡电路，**不能用于其他用途**（含 RC 32K 模式下同样不可用）。

### P1

不支持 FULLMUX 功能。

### P2、P3（SWD）

接调试器时非调试功能受影响，**生产版本硬件不要接调试器**。

---

## 3 GPIO 模式说明

### 3.1 上下拉电阻

| 配置 | 描述 | 电阻值 |
|------|------|--------|
| 浮空 | 高阻态 | — |
| 强上拉 | 上拉到 AVDD33，驱动电流大 | 150kΩ |
| 弱上拉 | 上拉到 AVDD33，驱动电流小 | 1MΩ |
| 下拉 | 下拉到 GND | 150kΩ |

默认下拉：P03、P24、P25；其余默认浮空。

### 3.2 Retention

- 默认关闭；开启后，**系统休眠时 GPIO 输出状态保持不变**
- 不开启时，休眠后 GPIO 恢复为输入态（驱动能力弱）

### 3.3 中断与唤醒

- 除 TEST_MODE、P16、P17 之外的所有 GPIO 均支持
- 中断：支持电平触发和边沿触发
- 唤醒：仅支持边沿触发
- ⚠️ 作为唤醒源时，**必须配置内部上拉或下拉，不能高阻态**

---

## 4 FULLMUX 复用表

不支持 FULLMUX 的引脚：TEST_MODE、P16、P17、P1。

| 枚举名 | 值 | 描述 |
|--------|----|------|
| FMUX_IIC0_SCL | 0 | I2C0 时钟 |
| FMUX_IIC0_SDA | 1 | I2C0 数据 |
| FMUX_IIC1_SCL | 2 | I2C1 时钟 |
| FMUX_IIC1_SDA | 3 | I2C1 数据 |
| FMUX_UART0_TX | 4 | UART0 TX |
| FMUX_UART0_RX | 5 | UART0 RX |
| FMUX_RF_RX_EN | 6 | RF RX 调试 |
| FMUX_RF_TX_EN | 7 | RF TX 调试 |
| FMUX_UART1_TX | 8 | UART1 TX |
| FMUX_UART1_RX | 9 | UART1 RX |
| FMUX_PWM0 | 10 | PWM 通道 0 |
| FMUX_PWM1 | 11 | PWM 通道 1 |
| FMUX_PWM2 | 12 | PWM 通道 2 |
| FMUX_PWM3 | 13 | PWM 通道 3 |
| FMUX_PWM4 | 14 | PWM 通道 4 |
| FMUX_PWM5 | 15 | PWM 通道 5 |
| FMUX_SPI_0_SCK | 16 | SPI0 时钟 |
| FMUX_SPI_0_SSN | 17 | SPI0 片选 |
| FMUX_SPI_0_TX | 18 | SPI0 TX |
| FMUX_SPI_0_RX | 19 | SPI0 RX |
| FMUX_SPI_1_SCK | 20 | SPI1 时钟 |
| FMUX_SPI_1_SSN | 21 | SPI1 片选 |
| FMUX_SPI_1_TX | 22 | SPI1 TX |
| FMUX_SPI_1_RX | 23 | SPI1 RX |
| FMUX_CHAX | 24 | 旋转编码器 CHAX |
| FMUX_CHBX | 25 | 旋转编码器 CHBX |
| FMUX_CHIX | 26 | 旋转编码器 CHIX |
| FMUX_CHAY | 27 | 旋转编码器 CHAY |
| FMUX_CHBY | 28 | 旋转编码器 CHBY |
| FMUX_CHIY | 29 | 旋转编码器 CHIY |
| FMUX_CHAZ | 30 | 旋转编码器 CHAZ |
| FMUX_CHBZ | 31 | 旋转编码器 CHBZ |
| FMUX_CHIZ | 32 | 旋转编码器 CHIZ |
| FMUX_CLK1P28M | 33 | DMIC 时钟 |
| FMUX_ADCC | 34 | DMIC 数据 |
| FMUX_ANT_SEL_0 | 35 | 天线选择 0（定位） |
| FMUX_ANT_SEL_1 | 36 | 天线选择 1（定位） |
| FMUX_ANT_SEL_2 | 37 | 天线选择 2（定位） |

---

## 5 ANALOG 模式

支持模拟功能的引脚：P11、P14、P15、P16、P17、P18、P20、P23、P24、P25。

- **晶振**：P16(XTALI) / P17(XTALO)，不可复用
- **ADC 单端**：P11、P14、P15、P20、P23、P24
- **ADC 差分**：P18/P25、P23/P11、P14/P24、P20/P15
- **AMIC**：P18(PGA+)、P20(PGA−)、P15(Mic Bias)、P23(Bias Ref，可选)

---

## 6 KSCAN 矩阵键盘

内置硬件矩阵扫描电路，最大支持 11 行 × 12 列。

**4×4 矩阵按键示例引脚分配：**

| 方向 | KSCAN | GPIO |
|------|-------|------|
| row0 | mk_in[0] | P0 |
| row1 | mk_in[1] | P2 |
| row2 | mk_in[2] | P15 |
| row3 | mk_in[3] | P25 |
| col0 | mk_out[0] | P1 |
| col1 | mk_out[1] | P3 |
| col2 | mk_out[2] | P14 |
| col3 | mk_out[3] | P24 |

按键矩阵（行×列 → 按键编号）：

```
       col0  col1  col2  col3
row0    S1    S2    S3    S4
row1    S5    S6    S7    S8
row2    S9   S10   S11   S12
row3   S13   S14   S15   S16
```

---

## 7 API 示例

### GPIO 输出

```c
// PHY6222/6252（推荐）
hal_gpio_pin_init(P0, GPIO_OUTPUT);
hal_gpio_fast_write(P0, 1);
hal_gpio_fast_write(P0, 0);

// PHY6220/6250
gpio_pin_handle_t h = csi_gpio_pin_initialize(P0, NULL);
csi_gpio_pin_config_direction(h, GPIO_DIRECTION_OUTPUT);
csi_gpio_pin_write(h, 1);
```

### GPIO 输入

```c
// PHY6222/6252
hal_gpio_pin_init(P0, GPIO_INPUT);
bool val = hal_gpio_read(P0);

// PHY6220/6250
gpio_pin_handle_t h = csi_gpio_pin_initialize(P0, NULL);
csi_gpio_pin_config_direction(h, GPIO_DIRECTION_INPUT);
bool val;
csi_gpio_pin_read(h, &val);
```

### GPIO Retention（休眠保持输出状态）

```c
// PHY6222/6252
hal_gpioretention_register(P0);   // 开启
hal_gpio_write(P0, 1);
// hal_gpioretention_unregister(P0); // 关闭

// PHY6220/6250
phy_gpioretention_register(P0);
csi_gpio_pin_write(h, 1);
```

### GPIO 上下拉

```c
// PHY6222/6252
hal_gpio_pull_set(P0, GPIO_PULL_UP_S); // 强上拉
hal_gpio_pin_init(P0, GPIO_INPUT);
bool val = hal_gpio_read(P0);

// PHY6220/6250：phy_gpio_pull_set(pin, GPIO_PULL_UP_S);
```
### GPIO 中断与唤醒

```c
// PHY6222/6252（中断+唤醒二合一）
void posedge_cb(GPIO_Pin_e pin, IO_Wakeup_Pol_e type) { /* 上升沿处理 */ }
void negedge_cb(GPIO_Pin_e pin, IO_Wakeup_Pol_e type) { /* 下降沿处理 */ }

hal_gpioin_register(P0, posedge_cb, negedge_cb); // 注册双沿回调

// PHY6220/6250：分别配置中断和唤醒
// drv_pinmux_config + csi_gpio_pin_set_irq（中断）
// phy_gpio_wakeup_set(pin, POL_RISING/POL_FALLING)（唤醒）
```

### FULLMUX 复用（以 UART 为例）

```c
// PHY6222/6252
uart_Cfg_t cfg = {
    .tx_pin    = P9,
    .rx_pin    = P10,
    .rts_pin   = GPIO_DUMMY,
    .cts_pin   = GPIO_DUMMY,
    .baudrate  = 115200,
    .use_fifo  = TRUE,
    .hw_fwctrl = FALSE,
    .use_tx_buf= FALSE,
    .parity    = FALSE,
    .evt_handler = NULL,
};
hal_uart_init(cfg, UART0);
// 驱动内部自动调用 hal_gpio_fmux_set(P9, FMUX_UART0_TX)

// PHY6220/6250
drv_pinmux_config(P9,  FMUX_UART0_TX);
drv_pinmux_config(P10, FMUX_UART0_RX);
console_init(0, 115200, 0);
```

### KSCAN 矩阵键盘（2×2 示例，PHY6222/6252）

```c
KSCAN_ROWS_e rows[] = {KEY_ROW_P23, KEY_ROW_P18};
KSCAN_COLS_e cols[] = {KEY_COL_P24, KEY_COL_P11};

static void kscan_evt_handler(kscan_Evt_t *evt)
{
    for (uint8_t i = 0; i < evt->num; i++) {
        LOG("row:%d col:%d %s\n",
            evt->keys[i].row, evt->keys[i].col,
            evt->keys[i].type == KEY_PRESSED ? "pressed" : "released");
    }
}

kscan_Cfg_t cfg = {
    .ghost_key_state = NOT_IGNORE_GHOST_KEY,
    .key_rows  = rows,
    .key_cols  = cols,
    .interval  = 50,
    .evt_handler = kscan_evt_handler,
};
hal_kscan_init(cfg, task_id, KSCAN_WAKEUP_TIMEOUT_EVT);
```