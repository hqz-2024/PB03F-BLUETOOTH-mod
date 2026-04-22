idf_component_register(
    SRCS "espidf_skills.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash driver esp_https_ota app_update esp_http_client
)
