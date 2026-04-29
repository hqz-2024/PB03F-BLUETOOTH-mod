---
description: LVGL Skills 模块 - 约束 AI 生成嵌入式 UI 代码的规范
---

# LVGL Skills 模块

## 1. 定义

- **LVGL 版本**: v8.3.10
- **屏幕**: 800x480, RGB565
- **内存池**: 320KB
- **用途**: 约束所有 LVGL UI 代码生成，确保工程化、可维护、可移植
- **适用场景**: ESP32-S3 / K230 / 其他 MCU 嵌入式小屏设备

---

## 2. 强制规则（CRITICAL）

- **禁止** 将 UI 代码写在 `app_main()` 或 `main()` 中，`app_main` 只负责初始化和启动
- **禁止** demo 风格代码（一个函数堆砌所有控件）
- **禁止** UI 与业务逻辑耦合，业务逻辑必须通过回调接口与 UI 交互
- **禁止** 在事件回调中编写复杂业务逻辑，回调只做：更新 UI 状态 或 发送消息/调用接口
- **禁止** 单文件包含多个 screen
- **每个界面必须独立为一个模块**（一个 .c + 一个 .h）
- **所有用户交互必须使用 `lv_obj_add_event_cb` 回调机制**

---

## 3. UI 架构

```
main/
├── APP/
│   └── ui/
│       ├── ui_manager.h        // 页面管理器声明
│       ├── ui_manager.c        // 页面切换、生命周期管理
│       ├── screens/
│       │   ├── scr_home.h
│       │   ├── scr_home.c      // 主页
│       │   ├── scr_settings.h
│       │   ├── scr_settings.c  // 设置页
│       │   └── ...
│       └── components/
│           ├── comp_btn.h
│           ├── comp_btn.c      // 通用按钮组件
│           ├── comp_topbar.h
│           ├── comp_topbar.c   // 顶部状态栏
│           └── ...
```

---

## 4. Screen 设计规范

每个 screen 模块必须实现以下接口：

```c
// scr_xxx.h
lv_obj_t *scr_xxx_create(void);     // 创建页面，返回 screen 对象
void scr_xxx_destroy(void);          // 销毁页面，释放资源
```

规则：
- `create()` 内用 `lv_obj_create(NULL)` 创建独立 screen
- `destroy()` 内用 `lv_obj_del()` 删除 screen 并置空指针
- 页面内部控件引用用 `static` 局部变量或结构体管理
- 事件回调函数声明为 `static`，放在同文件内

---

## 5. 组件设计规范

```c
// comp_btn.h - 可复用组件示例接口
lv_obj_t *comp_btn_create(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
```

规则：
- 组件必须接受 `parent` 参数
- 样式通过参数或配置结构体传入，不硬编码
- 组件不持有业务状态

---

## 6. 页面管理器（ui_manager）

```c
// ui_manager.h
typedef enum {
    SCR_ID_HOME,
    SCR_ID_SETTINGS,
    SCR_ID_MAX
} scr_id_t;

void ui_manager_init(void);                    // 初始化，加载首页
void ui_manager_switch(scr_id_t id);           // 切换页面（自动调用 destroy + create）
scr_id_t ui_manager_get_current(void);         // 获取当前页面 ID
```

规则：
- 切换页面时，先 `destroy` 旧页面，再 `create` 新页面
- 使用 `lv_scr_load_anim()` 或 `lv_scr_load()` 加载
- 页面注册表用函数指针数组实现

---

## 7. 事件系统

- 使用 `lv_obj_add_event_cb(obj, cb, event, user_data)`
- 回调签名: `static void xxx_event_cb(lv_event_t *e)`
- 回调内只做：
  - 读取事件数据 `lv_event_get_target(e)` / `lv_event_get_user_data(e)`
  - 更新 UI 状态
  - 调用业务接口函数（不在此实现业务逻辑）

---

## 8. 内存管理

- `lv_obj_create` 的对象必须有对应的 `lv_obj_del` 或随父对象销毁
- screen 切换时必须确保旧 screen 被删除
- 禁止在循环中反复 create 不 delete
- 动态分配的 `user_data` 需在 `LV_EVENT_DELETE` 回调中释放

---

## 9. 定时器 / 动画

- 使用 `lv_timer_create()` 创建定时器，页面销毁时用 `lv_timer_del()` 删除
- 动画使用 `lv_anim_t`，通过 `lv_anim_init()` + `lv_anim_start()` 创建
- 禁止在定时器回调中做阻塞操作

---

## 10. 代码风格

- 语言: C
- 文件命名: `scr_` 前缀为页面，`comp_` 前缀为组件，`ui_` 前缀为管理模块
- 函数命名: `模块名_动作`，如 `scr_home_create()`
- 静态函数用于内部回调和辅助函数

---

## 11. AI 使用说明

后续 AI 生成 LVGL 代码时 **必须**：
1. 遵循上述架构，按 `screens/` `components/` `ui_manager/` 组织代码
2. 每个页面独立文件，实现 `create()` / `destroy()` 接口
3. 通过 `ui_manager` 管理页面切换
4. 复用组件而非复制粘贴
5. UI 与业务解耦，通过回调接口通信

**禁止**：
- 写 demo 风格的一次性代码
- 将所有 UI 塞进一个文件
- 在回调中写业务逻辑
- 忽略内存释放
