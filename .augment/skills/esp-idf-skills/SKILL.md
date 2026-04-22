---
name: esp-idf-skills
version: 1.0.0
id: esp-idf-5.5.4-official-engineering-skill
description: ESP-IDF 5.5.4 工程级 Skills，强制官方 API、component+main 架构、可编译可复用代码生成。
compatible_with:
  - esp-idf@5.5.4
---

# ESP-IDF Skills 模块（ESP-IDF 5.5.4）

## 1. Skills 描述（给 AI 用）

### 作用
该 Skills 用于生成 **ESP-IDF 5.5.4 工程级代码**，确保代码满足官方推荐开发规范，默认使用官方 API、标准组件化结构、统一错误处理与日志机制，输出代码可直接 `idf.py build`。

### 适用范围
- WiFi（STA/AP，默认 STA）
- BLE（NimBLE / Bluedroid）
- GPIO（输入/输出/中断）
- OTA（`esp_https_ota`）
- NVS（`nvs_flash`）
- FreeRTOS（任务、队列、事件组、互斥量）
- 事件机制（`esp_event`）
- 网络基础（`esp_netif`）

### 使用约束
- 必须基于 **ESP-IDF 5.5.4**。
- 禁止生成 Arduino 风格 API。
- 禁止使用 legacy/过时初始化路径（除非用户明确要求）。
- 输出必须为工程代码风格，不得生成教学 Demo 话术或伪代码。
- 每个函数必须可复用，优先返回 `esp_err_t`。

---

## 2. 全局编码规范（强约束）

1. 必须使用 ESP-IDF 日志宏：`ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE`。
2. 禁止 Arduino 风格 API（如 `pinMode`/`digitalWrite`/`WiFi.begin`）。
3. 所有外设初始化必须使用官方 driver（如 `driver/gpio.h`）。
4. 错误处理必须使用 `ESP_ERROR_CHECK` 或标准错误码返回（`esp_err_t`）。
5. FreeRTOS 任务必须规范创建：`xTaskCreatePinnedToCore`（或 `tskNO_AFFINITY`）。
6. 禁止直接访问底层寄存器（除非用户特别说明且隔离到专用模块）。
7. 禁止把多模块逻辑全部塞入 `app_main`，必须模块化。

---

## 3. 项目结构模板（必须严格遵守）

```text
espidf_skills_project/
├─ CMakeLists.txt
├─ sdkconfig.defaults
├─ main/
│  ├─ CMakeLists.txt
│  └─ main.c
└─ components/
   └─ espidf_skills/
      ├─ CMakeLists.txt
      ├─ include/
      │  └─ espidf_skills.h
      └─ espidf_skills.c
```

---

## 4. 常用功能模板（可复用函数要求）

必须包含并优先复用以下接口：

- `skills_nvs_init()`：NVS 初始化
- `skills_event_loop_init()`：事件循环与网络栈初始化
- `skills_wifi_sta_start()`：WiFi STA 初始化与连接
- `skills_gpio_output_init()` / `skills_gpio_input_init()` / `skills_gpio_set_level()`：GPIO 输入输出
- `skills_task_create_pinned()`：FreeRTOS 任务规范创建
- `skills_ota_https_start()`：OTA 基础结构（`esp_https_ota`）

调用方（如 `main.c`）必须通过这些接口组装业务逻辑，不允许重复造轮子。

---

## 5. API 使用约束（非常重要）

- WiFi 必须使用：`esp_netif + esp_event + esp_wifi`
- 事件机制必须基于：`esp_event_handler_instance_register`
- 禁止直接操作寄存器（除非特别说明）
- BLE 必须使用 NimBLE 或 Bluedroid（**推荐 NimBLE**）
- OTA 必须使用官方 `esp_https_ota`
- 存储必须使用 `nvs_flash`

---

## 6. 代码风格要求

- 语言：C99
- 模块化函数设计
- 工程代码风格（非示例代码风格）
- 每个函数必须可复用
- 每个函数必须带简要注释（功能与用法）
- include 不允许省略
- 禁止伪代码

---

## 7. 输出格式约束（给 AI）

- 输出使用 Markdown
- 所有代码块必须完整可编译
- 必须包含完整 include
- 不写伪代码
- 目录结构与 CMake 依赖必须完整

---

## 生成策略（执行指令）

1. 优先生成组件：`components/<module>/include/*.h` + `*.c`
2. `main.c` 只做编排，不堆业务实现
3. 所有初始化函数可重复调用时需保证幂等或返回明确错误
4. 所有字符串参数做空指针和长度检查
5. 关键步骤打印日志，错误日志必须含 `esp_err_to_name(err)`
6. 可失败函数统一返回 `esp_err_t`
7. 仅在入口层使用 `ESP_ERROR_CHECK`

---

## 模板文件位置

本 Skills 附带完整模板工程，路径如下：

```text
templates/espidf_skills_project/
```

直接复制该目录到目标工作区后执行：

```bash
idf.py build
```
