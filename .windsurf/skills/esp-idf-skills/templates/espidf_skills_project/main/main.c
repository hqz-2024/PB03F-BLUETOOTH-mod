#include "espidf_skills.h"

#include "esp_log.h"

#ifndef CONFIG_APP_WIFI_SSID
#define CONFIG_APP_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef CONFIG_APP_WIFI_PASSWORD
#define CONFIG_APP_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef CONFIG_APP_LED_GPIO
#define CONFIG_APP_LED_GPIO 2
#endif

#ifndef CONFIG_APP_KEY_GPIO
#define CONFIG_APP_KEY_GPIO 0
#endif

static const char *TAG = "main";

/* LED 周期翻转任务。用法：验证 FreeRTOS 调度与 GPIO 输出。 */
static void app_led_task(void *arg)
{
    (void)arg;
    uint32_t level = 0;

    while (1) {
        level ^= 1U;
        ESP_ERROR_CHECK(skills_gpio_set_level((gpio_num_t)CONFIG_APP_LED_GPIO, level));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* 程序入口。用法：初始化基础模块并启动业务任务。 */
void app_main(void)
{
    ESP_ERROR_CHECK(skills_nvs_init());
    ESP_ERROR_CHECK(skills_wifi_sta_start(CONFIG_APP_WIFI_SSID, CONFIG_APP_WIFI_PASSWORD, 10));

    ESP_ERROR_CHECK(skills_gpio_output_init((gpio_num_t)CONFIG_APP_LED_GPIO, 0));
    ESP_ERROR_CHECK(skills_gpio_input_init((gpio_num_t)CONFIG_APP_KEY_GPIO, GPIO_PULLUP_ONLY));

    ESP_ERROR_CHECK(skills_task_create_pinned(
        app_led_task,
        "app_led_task",
        4096,
        NULL,
        5,
        NULL,
        tskNO_AFFINITY));

    ESP_LOGI(TAG, "application started");
}
