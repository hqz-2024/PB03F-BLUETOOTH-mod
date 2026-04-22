#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 NVS，自动处理无可用页和版本迁移场景。用法：系统启动时调用一次。 */
esp_err_t skills_nvs_init(void);

/* 初始化事件循环与网络栈。用法：任何网络功能启动前调用。 */
esp_err_t skills_event_loop_init(void);

/* 启动 WiFi STA 并等待连接。用法：传入 SSID/密码/重试次数，成功返回 ESP_OK。 */
esp_err_t skills_wifi_sta_start(const char *ssid, const char *password, int max_retry);

/* 查询当前 WiFi 是否已连接。用法：业务逻辑中轮询或状态判断。 */
bool skills_wifi_is_connected(void);

/* 初始化 GPIO 输出。用法：传入引脚和初始电平。 */
esp_err_t skills_gpio_output_init(gpio_num_t gpio_num, uint32_t initial_level);

/* 初始化 GPIO 输入。用法：传入引脚和上下拉模式。 */
esp_err_t skills_gpio_input_init(gpio_num_t gpio_num, gpio_pull_mode_t pull_mode);

/* 设置 GPIO 电平。用法：对已初始化输出引脚写入 0/1。 */
esp_err_t skills_gpio_set_level(gpio_num_t gpio_num, uint32_t level);

/* 规范创建 FreeRTOS 任务。用法：统一通过该接口创建并绑定核心。 */
esp_err_t skills_task_create_pinned(
    TaskFunction_t task_func,
    const char *name,
    uint32_t stack_depth,
    void *arg,
    UBaseType_t priority,
    TaskHandle_t *out_handle,
    BaseType_t core_id);

/* 启动 HTTPS OTA。用法：传入固件 URL 与服务器证书 PEM，成功后重启。 */
esp_err_t skills_ota_https_start(const char *url, const char *cert_pem);

#ifdef __cplusplus
}
#endif
