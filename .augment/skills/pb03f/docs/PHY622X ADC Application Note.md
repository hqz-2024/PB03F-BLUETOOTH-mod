# PHY622X ADC Application Note

> v1.0 | phyplusinc | 2021.01.25 | 适用于：PHY6220 / PHY6250 / PHY6222 / PHY6252

---

## 1 引脚说明

ADC 使用时 IO 口工作在模拟模式。以 QFN32 为例，支持模拟功能的引脚如下：

| 引脚 | 默认模式 | 方向 | 支持模拟 |
|------|---------|------|---------|
| P11  | GPIO    | IN   | ✓ |
| P14  | GPIO    | IN   | ✓ |
| P15  | GPIO    | IN   | ✓ |
| P16  | XTALI   | ANA  | ✓（不可用于ADC，接晶振） |
| P17  | XTALO   | ANA  | ✓（不可用于ADC，接晶振） |
| P18  | GPIO    | IN   | ✓ |
| P20  | GPIO    | IN   | ✓ |
| P23  | GPIO    | IN   | ✓ |
| P24  | GPIO    | IN   | ✓ |
| P25  | GPIO    | IN   | ✓ |

**引脚用途分类：**

- **ADC 单端**：P11、P14、P15、P20、P23、P24
- **ADC 差分**：P18(+)/P25(−)、P23(+)/P11(−)、P14(+)/P24(−)、P20(+)/P15(−)
- **Voice PGA**：P18(PGA+)、P20(PGA−)、P15(Mic Bias)、P23(Mic Bias Ref，可选)

---

## 2 ADC 概述

- **基准电压**：内部 0.8V
- **分辨率**：12bit SAR ADC
- **可用通道**：8 个

### 2.1 输入模式

| 模式 | 描述 |
|------|------|
| 单端 | 测量引脚与 GND 之间的电压 |
| 差分 | 测量两个引脚之间的电压 |

### 2.2 量程模式

| 模式 | 原理 | 量程 | 精度 |
|------|------|------|------|
| **bypass** | 引脚电压直接进入 ADC | 0 ~ 0.8V | ~0.2mV（0.8/4096） |
| **attenuation** | 内部 4:1 分压（14.15kΩ + 4.72kΩ）| 0 ~ 3.2V | 内部相对误差 ±1% |

> ⚠️ 不能同时使用内部分压（attenuation）和外部分压电阻。

### 2.3 工作时钟

| HCLK | ADC 时钟 |
|------|---------|
| 32MHz / 64MHz | 1.28MHz |
| 其他 | 1MHz |

### 2.4 扫描模式

- **手动模式**：一次采集一个单端通道或一组差分通道
- **自动模式**：SDK 默认，自动扫描所有已使能通道并存储结果

---

## 3 ADC 硬件注意事项

| 采集电压范围 | 推荐方案 |
|------------|---------|
| < 0.8V | 直接使用 bypass 模式，引脚加滤波电容 |
| 0.8V ~ 3.2V | 使用 attenuation 模式，或外部电阻分压 + bypass |
| > 3.2V | 必须外部电阻分压 + bypass 模式，引脚加滤波电容 |

**外部分压电路约束（bypass 模式）：**

```
Vin ──R1──┬── AIO(PHY62XX) ──C── GND
          R2
          │
         GND
```

- 检测电压需满足：`Vin × R2/(R1+R2) < 0.8V`
- 增益：`Gain = R2 / (R1 + R2)`
- 驱动阻抗：`R1 // R2 // C` 需满足 ADC 采样要求

---

## 4 ADC 软件注意事项

### 4.1 PHY6222 / PHY6252（推荐）

使用 `adc_Cfg_t` 结构体配置：

```c
typedef struct _adc_Cfg_t {
    uint8_t channel;              // 通道选择，每 bit 对应一个通道
    bool    is_continue_mode;     // TRUE=持续采集，FALSE=单次采集后关闭
    uint8_t is_differential_mode; // 0=单端，非0=差分
    uint8_t is_high_resolution;   // 每bit对应通道: 0=attenuation, 1=bypass
} adc_Cfg_t;
```

参考 `example/peripheral/adc`。

### 4.2 PHY6220 / PHY6250（旧版）

使用 `drv_adc_*` 系列接口，轮询方式读取 ADC 值：

```c
// P11 检测芯片电源电压，P20 检测外部ADC
uint32_t ch_adc[] = {ADC_CH1N_P11, ADC_CH3P_P20};
adc_conf_t adc_config = {
    .mode        = ADC_SCAN,
    .intrp_mode  = 0,
    .channel_array = ch_adc,
    .channel_nbr = 2,
    .conv_cnt    = 10,
};
adc_handle_t hd = drv_adc_initialize(0, NULL);
drv_adc_battery_config(hd, &adc_config, ADC_CH1N_P11); // 用drv_adc_config则不检测电源
drv_adc_start(hd);
mdelay(10);
uint32_t adc_data[2];
drv_adc_read(hd, adc_data, 2);  // 结果单位 mV
drv_adc_stop(hd);
drv_adc_uninitialize(hd);
```

---

## 5 ADC 校准原理

attenuation 模式下，各通道到 ADC 模块的寄生电阻不同（与 IC 走线相关），会影响实际衰减系数。驱动中使用 `adc_Lambda` 来补偿寄生电阻的影响。

**校准方法：**
1. bypass 模式，输入电压 Vin1，输出 Vout1
2. attenuation 模式，输入 Vin2 输出 Vout2
3. 衰减系数 `λ = (Vin2 × Vout1) / (Vin1 × Vout2)`

**PHY6222 单端模式各通道衰减系数参考值（bypass 400mV，attenuation 1602mV）：**

| 引脚 | bypass Vout (mV) | attenuation Vout (mV) | 衰减系数 λ | Vin/Vout 比 |
|------|-----------------|----------------------|-----------|------------|
| P11  | 2081.5 | 1844.5 | 4.5196 | 0.868 |
| P23  | 2128.5 | 1978.5 | 4.3086 | 0.809 |
| P24  | 2071.5 | 1946.0 | 4.2633 | 0.823 |
| P14  | 2133.6 | 1906.2 | 4.4827 | 0.840 |
| P15  | 2096.9 | 2008.9 | 4.1804 | 0.797 |
| P20  | 2125.0 | 2090.0 | 4.0721 | 0.766 |

> 查看 `phy_adc.c`（6220/6250）或 `adc.c`（6222/6252）中是否包含 `adc_Lambda`，包含则已考虑寄生电阻补偿。

---

## 6 Voice 概述

支持 DMIC（SAR-ADC）和 AMIC（差分/单端），采样率：8K / 16K / 32K / 64K，要求 HCLK ≥ 16MHz。

### 6.1 Voice 模式对比

| 模式 | 原理 | 引脚 |
|------|------|------|
| DMIC | 外部数字麦克风，引脚灵活复用 | `clk_1p28m`、`adcc_dmic_out`（任意 GPIO FMUX） |
| AMIC | 内部 PGA + ADC，引脚固定 | P18(PGA+)、P20(PGA−)、P15(Mic Bias)、P23(Bias Ref，可选) |

> Memory buffer：`0x4005800 ~ 0x4005BFF`（1024 Byte / 256 Word），高 16bit = 左通道，每 128 采样点触发一次中断。

### 6.2 Voice 硬件参考电路

**DMIC（以 SPM0423HM4H-WB 为例）：**

```
VDD3.3MIC ── VDD
GND       ── GND
D2        ── DATA
D3        ── CLK
```

**双端 AMIC：**

```
P23 ── C13(1µF) ── GND
P15 ── R10(200Ω) ── C14(10µF) ── AMIC_VDD
                               └── C15(0.1µF) ── GND
P20 ── C16(1µF) ── AMIC_OUT−  ── C17(2.2nF) ── GND
P18 ── C20(1µF) ── AMIC_OUT+  ── R18(4.7kΩ) ── C21(2.2nF) ── GND
```

**单端 AMIC（P18 耦合电容不可省）：**

```
P23 ── C13(1µF) ── GND
P15 ── R10(200Ω) ── C14(10µF) ── AMIC_VDD
                               └── C15(0.1µF) ── GND
P20 ── C16(1µF) ── AMIC_OUT   ── C17(2.2nF) ── GND
P18 ── C20(1µF) ── GND
```

### 6.3 Voice 软件配置

```c
typedef struct _voice_Cfg_t {
    bool           voiceSelAmicDmic;  // FALSE=AMIC，TRUE=DMIC
    gpio_pin_e     dmicDataPin;       // DMIC 数据引脚（AMIC时无效）
    gpio_pin_e     dmicClkPin;        // DMIC 时钟引脚（AMIC时无效）
    uint8_t        amicGain;          // AMIC PGA 增益（两级可配）
    uint8_t        voiceGain;         // Voice 增益，-20dB~+20dB，step=0.5dB
    VOICE_ENCODE_t voiceEncodeMode;   // 编码方式（PCM/ADPCM等）
    VOICE_RATE_t   voiceRate;         // 采样率：8K/16K/32K/64K
    bool           voiceAutoMuteOnOff;// 自动静音，默认0（支持）
} voice_Cfg_t;

// 初始化 Voice，注册回调
hal_voice_config(cfg, voice_evt_handler);
```

**数据回调示例：**

```c
static void voice_evt_handler(voice_Evt_t *pev)
{
    if (pev->type == HAL_VOICE_EVT_DATA) {
        for (uint32_t i = 0; i < pev->size; i++) {
            uint32_t dual    = pev->data[i];
            int16_t  left    = (int16_t)((dual >> 16) & 0xFFFF);
            int16_t  right   = (int16_t)(dual & 0xFFFF);
            // 处理 left / right 采样数据
        }
    }
}
```
