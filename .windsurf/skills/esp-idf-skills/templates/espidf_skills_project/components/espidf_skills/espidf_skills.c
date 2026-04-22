#include "espidf_skills.h"

#include <string.h>
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "espidf_skills";

static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_connected;
static int s_wifi_retry_count;
static int s_wifi_max_retry;
static esp_event_handler_instance_t s_wifi_any_handler;
static esp_event_handler_instance_t s_ip_got_ip_handler;

/* 统一处理 WiFi 与 IP 事件。用法：内部注册到 esp_event。 */
static void skills_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "wifi start, connecting");
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_wifi_retry_count < s_wifi_max_retry) {
            s_wifi_retry_count++;
            ESP_LOGW(TAG, "wifi disconnected, retry %d/%d", s_wifi_retry_count, s_wifi_max_retry);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "wifi connect failed after retries");
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry_count = 0;
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "wifi got ip");
    }
}

esp_err_t skills_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t skills_event_loop_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    return ESP_OK;
}

esp_err_t skills_wifi_sta_start(const char *ssid, const char *password, int max_retry)
{
    if (ssid == NULL || password == NULL || max_retry < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strlen(ssid);
    size_t pwd_len = strlen(password);

    if (ssid_len == 0 || ssid_len > 32 || pwd_len > 64) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = skills_event_loop_init();
    if (err != ESP_OK) {
        return err;
    }

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    } else {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    s_wifi_connected = false;
    s_wifi_retry_count = 0;
    s_wifi_max_retry = max_retry;

    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) {
        if (esp_netif_create_default_wifi_sta() == NULL) {
            return ESP_FAIL;
        }
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &skills_wifi_event_handler,
        NULL,
        &s_wifi_any_handler);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &skills_wifi_event_handler,
        NULL,
        &s_ip_got_ip_handler);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    wifi_config_t wifi_cfg = {0};
    memcpy(wifi_cfg.sta.ssid, ssid, ssid_len);
    memcpy(wifi_cfg.sta.password, password, pwd_len);
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;
    wifi_cfg.sta.threshold.authmode = (pwd_len == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(30000));

    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        return ESP_OK;
    }

    if ((bits & WIFI_FAIL_BIT) != 0) {
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
}

bool skills_wifi_is_connected(void)
{
    return s_wifi_connected;
}

esp_err_t skills_gpio_output_init(gpio_num_t gpio_num, uint32_t initial_level)
{
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_set_level(gpio_num, initial_level ? 1 : 0);
}

esp_err_t skills_gpio_input_init(gpio_num_t gpio_num, gpio_pull_mode_t pull_mode)
{
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    switch (pull_mode) {
        case GPIO_PULLUP_ONLY:
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case GPIO_PULLDOWN_ONLY:
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        case GPIO_PULLUP_PULLDOWN:
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        case GPIO_FLOATING:
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return gpio_config(&cfg);
}

esp_err_t skills_gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_level(gpio_num, level ? 1 : 0);
}

esp_err_t skills_task_create_pinned(
    TaskFunction_t task_func,
    const char *name,
    uint32_t stack_depth,
    void *arg,
    UBaseType_t priority,
    TaskHandle_t *out_handle,
    BaseType_t core_id)
{
    if (task_func == NULL || name == NULL || stack_depth == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (core_id != tskNO_AFFINITY && (core_id < 0 || core_id >= configNUM_CORES)) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(task_func, name, stack_depth, arg, priority, out_handle, core_id);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t skills_ota_https_start(const char *url, const char *cert_pem)
{
    if (url == NULL || strlen(url) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t http_cfg = {
        .url = url,
        .cert_pem = cert_pem,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    ESP_LOGI(TAG, "ota start: %s", url);
    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ota success, restarting");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "ota failed: %s", esp_err_to_name(err));
    }

    return err;
}
