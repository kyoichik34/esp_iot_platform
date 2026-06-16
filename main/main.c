#include <stdio.h>
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "config.h"
#include "wifi.h"
#include "ui.h"

static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&conf);
}

void app_main(void)
{
    nvs_flash_init();

    spiffs_init();

    config_load();

    ui_init();
    ui_update("Booting...");

    wifi_init_sta();

    ui_update("WiFi connecting...");
}
