# PHY62XX BSP_Button Application Note

> v1.1 | phyplusinc | 2021.07.20 | 适用于：PHY6220 / PHY6250 / PHY6222 / PHY6252

BSP_Button 是按键中间件，依赖 GPIO / KSCAN 硬件，通过 OSAL Task 调度实现按键事件检测。

---

## 1 源文件

| 文件 | 说明 |
|------|------|
| `bsp_gpio.c` | GPIO 按键驱动（仅 GPIO 按键时包含） |
| `kscan.c` | KSCAN 按键驱动（仅 KSCAN 按键时包含） |
| `bsp_button.c` | 按键检测逻辑 |
| `bsp_button_task.c` | OSAL 调度机制，提供 `Bsp_Btn_Init` / `Bsp_Btn_ProcessEvent` |

---

## 2 按键数据格式

按键事件类型 `uint8_t`：
- **高 2bit**：按键类型（`BSP_BTN_TYPE(evt)`）
- **低 6bit**：按键值/索引 0x00~0x2F（`BSP_BTN_INDEX(evt)`，0x30~0x3F 系统保留）

| 类型宏 | 描述 |
|--------|------|
| `BSP_BTN_PD_TYPE` | 按下 |
| `BSP_BTN_UP_TYPE` | 释放 |
| `BSP_BTN_LPS_TYPE` | 长按开始 |
| `BSP_BTN_LPK_TYPE` | 长按保持 |

---

## 3 配置宏

### 硬件类型（`BSP_BTN_HARDWARE_CONFIG`）

| 宏 | 说明 |
|----|------|
| `BSP_BTN_JUST_GPIO` | 只用 GPIO |
| `BSP_BTN_JUST_KSCAN` | 只用 KSCAN |
| `BSP_BTN_GPIO_AND_KSCAN` | GPIO + KSCAN 混合 |

### 按键数量

| 宏 | 说明 |
|----|------|
| `BSP_SINGLE_BTN_NUM` | 单独按键数量 |
| `BSP_COMBINE_BTN_NUM` | 组合按键数量（无则为 0） |
| `BSP_TOTAL_BTN_NUM` | 总数 = 单独 + 组合 |
| `BSP_KSCAN_SINGLE_BTN_NUM` | KSCAN 单独按键（混合模式） |
| `BSP_GPIO_SINGLE_BTN_NUM` | GPIO 单独按键（混合模式） |
| `BSP_KSCAN_COMBINE_BTN_NUM` | KSCAN 组合按键（混合模式） |
| `BSP_GPIO_COMBINE_BTN_NUM` | GPIO 组合按键（混合模式） |

> 混合模式按键排列顺序：**KSCAN 单独** → **GPIO 单独** → **组合按键**

### 扩展功能

| 宏 | 说明 |
|----|------|
| `BSP_BTN_LONG_PRESS_ENABLE` | 定义后启用长按（LPS + LPK） |
| `BTN_SYS_TICK` | 调度周期（ms） |
| `BTN_FILTER_TICK_COUNT` | 消抖时间 = `BTN_SYS_TICK × BTN_FILTER_TICK_COUNT` |
| `BTN_LONG_PRESS_START_TICK_COUNT` | 长按开始时间 |
| `BTN_LONG_PRESS_KEEP_TICK_COUNT` | 长按保持间隔 |

默认最多支持 **48 个按键**，超出需修改代码。

---

## 4 应用示例

### 4.1 只有 GPIO（3 单独 + 2 组合）

按键值映射：0=P14，1=P15，2=P26，3=P14+P15，4=P14+P26

```c
// bsp_button_task.h
#define BSP_BTN_HARDWARE_CONFIG BSP_BTN_JUST_GPIO
#define BSP_SINGLE_BTN_NUM      (GPIO_SINGLE_BTN_NUM)
#define BSP_COMBINE_BTN_NUM     (2)
#define BSP_TOTAL_BTN_NUM       (BSP_SINGLE_BTN_NUM + BSP_COMBINE_BTN_NUM)

// bsp_gpio.h
#define GPIO_SINGLE_BTN_NUM       3
#define GPIO_SINGLE_BTN_IDLE_LEVEL 1

// bsp_button_task.h
#define BSP_BTN_HARDWARE_CONFIG BSP_BTN_JUST_GPIO
#define BSP_SINGLE_BTN_NUM (GPIO_SINGLE_BTN_NUM)
#define BSP_COMBINE_BTN_NUM (2)
#define BSP_TOTAL_BTN_NUM (BSP_SINGLE_BTN_NUM + BSP_COMBINE_BTN_NUM)

// bsp_gpio.h
#define GPIO_SINGLE_BTN_NUM 3
#define GPIO_SINGLE_BTN_IDLE_LEVEL 1

// bsp_btn_demo.c（GPIO 示例）
uint32_t usr_combine_btn_array[] = { BIT(0)|BIT(1), BIT(0)|BIT(2) };

void hal_bsp_btn_callback(uint8_t evt)
{
    switch (BSP_BTN_TYPE(evt)) {
        case BSP_BTN_PD_TYPE:  LOG("press down ");       break;
        case BSP_BTN_UP_TYPE:  LOG("press up ");         break;
        case BSP_BTN_LPS_TYPE: LOG("long press start "); break;
        case BSP_BTN_LPK_TYPE: LOG("long press keep ");  break;
    }
    LOG("value:%d\n", BSP_BTN_INDEX(evt));
}

Gpio_Btn_Info gpio_btn_info = { {P14, P15, P26}, hal_bsp_btn_callback };

void Demo_Init(uint8 task_id)
{
    if (PPlus_SUCCESS == hal_gpio_btn_init(&gpio_btn_info))
        bsp_btn_gpio_flag = TRUE;
}
```

### 4.2 只有 KSCAN（4×4 = 16 单独 + 2 组合）

```c
// bsp_button_task.h
#define BSP_BTN_HARDWARE_CONFIG BSP_BTN_JUST_KSCAN
#define BSP_SINGLE_BTN_NUM  (NUM_KEY_ROWS * NUM_KEY_COLS)
#define BSP_COMBINE_BTN_NUM (2)
#define BSP_TOTAL_BTN_NUM   (BSP_SINGLE_BTN_NUM + BSP_COMBINE_BTN_NUM)

// kscan.h
#define NUM_KEY_ROWS 4
#define NUM_KEY_COLS 4

// bsp_btn_demo.c
KSCAN_ROWS_e rows[] = {KEY_ROW_P00, KEY_ROW_P02, KEY_ROW_P25, KEY_ROW_P18};
KSCAN_COLS_e cols[] = {KEY_COL_P01, KEY_COL_P03, KEY_COL_P24, KEY_COL_P20};
uint32_t usr_combine_btn_array[] = { BIT(0)|BIT(1), BIT(2)|BIT(3) };

void Demo_Init(uint8 task_id)
{
    hal_kscan_btn_check(hal_bsp_btn_callback);
}
```

### 4.3 GPIO + KSCAN 混合（4×4 KSCAN + 3 GPIO + 2 组合）

按键值顺序：0~15 = KSCAN 单独，16~18 = GPIO 单独，19 = KSCAN 组合，20 = GPIO 组合

```c
// bsp_button_task.h
#define BSP_BTN_HARDWARE_CONFIG  BSP_BTN_GPIO_AND_KSCAN
#define BSP_KSCAN_SINGLE_BTN_NUM (NUM_KEY_ROWS * NUM_KEY_COLS)
#define BSP_GPIO_SINGLE_BTN_NUM  (GPIO_SINGLE_BTN_NUM)
#define BSP_KSCAN_COMBINE_BTN_NUM (1)
#define BSP_GPIO_COMBINE_BTN_NUM  (1)
#define BSP_SINGLE_BTN_NUM  (BSP_KSCAN_SINGLE_BTN_NUM + BSP_GPIO_SINGLE_BTN_NUM)
#define BSP_COMBINE_BTN_NUM (BSP_KSCAN_COMBINE_BTN_NUM + BSP_GPIO_COMBINE_BTN_NUM)
#define BSP_TOTAL_BTN_NUM   (BSP_SINGLE_BTN_NUM + BSP_COMBINE_BTN_NUM)

// bsp_btn_demo.c
uint32_t usr_combine_btn_array[] = { BIT(2)|BIT(3), BIT(16)|BIT(17) };

void Demo_Init(uint8 task_id)
{
    hal_kscan_btn_check(hal_bsp_btn_callback);
    if (PPlus_SUCCESS == hal_gpio_btn_init(&gpio_btn_info))
        bsp_btn_gpio_flag = TRUE;
}
```