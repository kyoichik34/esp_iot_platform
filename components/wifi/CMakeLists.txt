idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_http_client nvs_flash config ui http presence
)