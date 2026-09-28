idf_component_register(
    SRCS "metrics.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server esp_timer
    PRIV_REQUIRES esp_system cjson
)