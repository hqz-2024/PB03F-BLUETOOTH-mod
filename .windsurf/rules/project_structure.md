---
type: "always_apply"
trigger: manual
---

# 工程结构总览

**项目名称**：`lvgl_demo`  
**目标芯片**：ESP32-S3  
**显示屏**：UEDX80480050（5寸，800×480）  
**框架**：ESP-IDF + LVGL v9  

---

## 目录树

```
lvgl_v9_port/
├── CMakeLists.txt                  # 顶层构建文件，指定 components 目录
├── partitions.csv                  # 分区表
├── sdkconfig                       # menuconfig 生成的配置
├── dependencies.lock               # 组件依赖锁定文件
│
├── main/                           # 应用层（用户代码）
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild           # 工程级 Kconfig 菜单
│   ├── idf_component.yml           # 组件依赖声明
│   ├── main.c                      # 入口，LVGL UI 示例
│   ├── board.c                     # 板级初始化（LCD / Touch / 背光）
│   └── board.h
│
├── components/                     # 本地组件（随工程一起维护）
│   ├── espressif__esp_lcd_touch_cst816s/   # 触摸驱动：CST816S
│   ├── espressif__esp_lcd_touch_ft5x06/    # 触摸驱动：FT5x06
│   ├── espressif__esp_lcd_touch_gt911/     # 触摸驱动：GT911
│   ├── espressif__esp_lvgl_port/           # LVGL 移植层（显示/触摸/按键接口）
│   └── lvgl/                               # LVGL v9 图形库源码
│
└── managed_components/             # IDF Component Manager 自动下载的组件
    ├── espressif__esp_lcd_touch/           # 触摸抽象层（统一 API）
    ├── espressif__button/                  # 按键驱动
    └── espressif__cmake_utilities/         # CMake 辅助工具
```

---

## 驱动程序路径

### 显示屏驱动

> **本屏幕采用 16-bit RGB 并行接口（RGB565），无独立 IC 驱动芯片组件。**  
> 显示驱动由 **ESP-IDF 内置组件 `esp_lcd_panel_rgb`** 提供，通过 `esp_lcd_panel_rgb.h` 引入，调用入口为 `esp_lcd_new_rgb_panel()`。

| 层次 | 路径 / 来源 | 关键文件 |
|------|------------|---------|
| RGB Panel 驱动（ESP-IDF 内置） | `$IDF_PATH/components/esp_lcd/` | `esp_lcd_panel_rgb.h` / `esp_lcd_panel_ops.h` |
| LVGL 显示移植层（lvgl9） | `components/espressif__esp_lvgl_port/src/lvgl9/` | `esp_lvgl_port_disp.c` |
| LVGL 自定义显示端口 | `components/lvgl/porting/` | `lv_port_disp.c` / `lv_port_disp.h` |
| 板级初始化（RGB 时序 + 引脚） | `main/` | `board.c` / `board.h` |

**RGB 时序参数**（定义于 `main/board.h`）：

| 分辨率 | pclk | hsync bp/fp/pw | vsync bp/fp/pw |
|--------|------|----------------|----------------|
| 800×480 | 15 MHz | 42 / 20 / 1 | 12 / 4 / 10 |
| 480×272 | 9 MHz  | 43 / 8 / 4  | 12 / 8 / 4  |

---

### 触摸屏驱动

| 驱动芯片 | 路径 | 关键文件 |
|---------|------|---------|
| CST816S | `components/espressif__esp_lcd_touch_cst816s/` | `esp_lcd_touch_cst816s.c` / `include/esp_lcd_touch_cst816s.h` |
| FT5x06  | `components/espressif__esp_lcd_touch_ft5x06/`  | `esp_lcd_touch_ft5x06.c` / `include/esp_lcd_touch_ft5x06.h`   |
| GT911   | `components/espressif__esp_lcd_touch_gt911/`   | `esp_lcd_touch_gt911.c` / `include/esp_lcd_touch_gt911.h`     |

### 触摸抽象层（统一接口）

| 组件 | 路径 | 关键文件 |
|------|------|---------|
| esp_lcd_touch | `managed_components/espressif__esp_lcd_touch/` | `esp_lcd_touch.c` / `include/esp_lcd_touch.h` |

### LVGL 移植层（显示 + 触摸 + 按键桥接）

| 组件 | 路径 |
|------|------|
| esp_lvgl_port | `components/espressif__esp_lvgl_port/` |

**源文件**（`src/lvgl9/`）：

- `esp_lvgl_port.c` — 核心初始化与任务管理
- `esp_lvgl_port_disp.c` — 显示驱动适配（RGB / SPI / I80）
- `esp_lvgl_port_touch.c` — 触摸输入适配
- `esp_lvgl_port_button.c` — 物理按键输入适配
- `esp_lvgl_port_knob.c` — 旋转编码器适配
- `esp_lvgl_port_usbhid.c` — USB HID 输入适配

**头文件**（`include/`）：

- `esp_lvgl_port.h`
- `esp_lvgl_port_disp.h`
- `esp_lvgl_port_touch.h`
- `esp_lvgl_port_button.h`
- `esp_lvgl_port_knob.h`
- `esp_lvgl_port_usbhid.h`
- `esp_lvgl_port_compatibility.h`

### 按键驱动

| 组件 | 路径 | 关键文件 |
|------|------|---------|
| button | `managed_components/espressif__button/` | `iot_button.c` / `button_gpio.c` / `button_adc.c` / `button_matrix.c` |

### LVGL 图形库

| 组件 | 路径 |
|------|------|
| lvgl v9 | `components/lvgl/` |

### 工具组件

| 组件 | 路径 | 说明 |
|------|------|------|
| cmake_utilities | `managed_components/espressif__cmake_utilities/` | OTA 压缩、单 bin 生成、重链接等 CMake 脚本 |

---

## 板级入口文件

| 文件 | 说明 |
|------|------|
| `main/board.h` | 引脚宏定义、LCD / Touch 配置参数 |
| `main/board.c` | 板级硬件初始化（SPI/RGB LCD、I2C Touch、背光 PWM） |
| `main/main.c`  | FreeRTOS 入口，LVGL UI 创建与事件循环 |
