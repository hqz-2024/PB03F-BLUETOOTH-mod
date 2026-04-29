---
name: lvgl-skills
description: LVGL Skills 模块 - 约束 AI 生成嵌入式 UI 代码的规范
---

# LVGL Skills 模块

## 1. 定义

- **默认参考参数**: 800x480, RGB565, 内存池 320KB（用于初始开发，不作为分辨率绑定）
- **用途**: 约束 LVGL UI 代码生成，确保工程化、可维护、可移植
- **适用场景**: ESP32-S3 / K230 / 其他 MCU 嵌入式小屏设备；同一套 UI 需可适配多种分辨率屏幕

## 2. CRITICAL（唯一禁止清单）
- **禁止** 将 UI 代码写在 `app_main()` 或 `main()` 中
- **禁止** demo 风格代码（单函数堆砌控件）
- **禁止** UI 与业务逻辑耦合
- **禁止** 在事件回调中编写复杂业务逻辑
- **禁止** 单文件包含多个 screen
- **禁止** 写死像素位置、固定宽高、魔法数布局
- **禁止** 使用不基于屏幕参数的绝对坐标布局

## 3. 架构

### 3.1 工程基线（官方开发板 + GUI Guider 可移植）
- 新项目默认沿用本工程架构，确保可直接套用 GUI Guider 生成代码
- 允许保留 `generated` 自动生成层与 `custom` 手写扩展层并行
- UI 业务扩展优先写在手写层，避免直接改动生成文件导致下次导出覆盖

```text
main/
├── APP/
│   ├── lvgl_demo.c                 # LVGL初始化、tick、主循环
│   └── ui/                         # 项目UI手写层（建议）
│       ├── ui_manager.h
│       ├── ui_manager.c
│       ├── screens/
│       └── components/

components/
└── BSP/
    ├── generated/                  # GUI Guider 导出层（setup_scr_xxx / events_init / gui_guider）
    └── custom/                     # 手写扩展层（定时器、业务桥接、二次封装）
```

### 3.2 GUI Guider 兼容约束
- GUI Guider 生成文件作为 UI 结构基底，负责控件创建和基础事件挂载
- 手写代码通过 `custom` 或 `main/APP/ui` 对生成层进行调用与扩展
- 页面切换可复用生成层的 `ui_load_scr_animation(...)`，或在 `ui_manager` 中统一封装
- 生成层与手写层之间只通过明确接口交互，避免跨层直接修改内部对象

### 布局规范（分辨率自适应）
- 所有布局必须由 `screen_width`、`screen_height` 驱动
- 必须定义 `padding_left/right/top/bottom`
- 统一可用区域：`usable_width = screen_width - padding_left - padding_right`
- 位置与尺寸必须通过计算得到，优先相对布局与对齐布局
- 文本必须按可用宽度约束，并在必要时调用 `lv_obj_update_layout()`
- 动画位移必须基于屏幕尺寸或元素尺寸计算
- 必须支持不同分辨率（如 240x240 / 320x240 / 480x320 / 800x480）、不同 padding、不同字体大小
- GUI Guider 导出后如出现固定坐标/固定尺寸，必须在手写层进行参数化与比例化改造后再用于项目

## 4. Screen
- 每个界面必须独立模块（一个 `.c` + 一个 `.h`)
- 必须包含 `create()`、`destroy()`、`event_handler()`

```c
lv_obj_t *scr_xxx_create(void);
void scr_xxx_destroy(void);
static void scr_xxx_event_handler(lv_event_t *e);
```

## 5. Component
- 组件必须可复用
- 组件必须参数化创建
- 组件接口必须接受 `parent` 参数

```c
lv_obj_t *comp_btn_create(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
```

## 6. ui_manager
- 负责页面初始化、切换、生命周期管理
- 切换流程：`destroy(old)` -> `create(new)` -> `lv_scr_load()/lv_scr_load_anim()`
- 推荐函数指针注册表管理页面

## 7. 事件
- 所有用户交互使用 `lv_obj_add_event_cb()`
- 回调签名：`static void xxx_event_cb(lv_event_t *e)`
- 回调只做事件识别、UI 更新、业务接口调用

## 8. 内存
- `lv_obj_create` 创建对象必须可回收
- 页面切换必须确保旧页面已释放
- 动态 `user_data` 在 `LV_EVENT_DELETE` 中释放
- 页面销毁时同步释放相关定时器与动画资源

## 9. 定时器动画
- 使用 `lv_timer_create()` / `lv_timer_del()` 成对管理
- 动画使用 `lv_anim_t` + `lv_anim_init()` + `lv_anim_start()`
- 定时器回调禁止阻塞

## 10. 代码风格
- 语言：C
- 文件命名：`scr_` 页面，`comp_` 组件，`ui_` 管理模块
- 函数命名：`模块名_动作`，如 `scr_home_create()`
- 内部辅助函数与回调使用 `static`

## 11. AI 约束
- 必须复用 `screens/` + `components/` + `ui_manager/` 架构
- 必须兼容“官方开发板工程 + GUI Guider generated/custom 分层”
- 必须确保同一套 UI 代码可适配多种分辨率屏幕
- 必须实现页面模块化与生命周期接口
- 必须遵循 `CRITICAL` 唯一禁止清单
