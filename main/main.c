#include <stdio.h>

#include "config.h"
#include "wifi.h"
#include "ui.h"
#include "presence.h"

#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_http_client.h"

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

void sync_task(void *arg)
{
    while (1) {
        vTaskDelay(g_config.sync_interval * 1000 / portTICK_PERIOD_MS);

        if (g_config.is_slave) {
            presence_poll_master();     // ★pull
        } else {
            if (g_config.send_update) {
                presence_send_update_all(); // ★heartbeat
            }
        }
    }
}

void app_main(void)
{
    nvs_flash_init();

    spiffs_init();

    config_init();

    if (!config_valid) {
        ESP_LOGE("main", "CONFIG ERROR");
        ui_update("CONFIG ERROR");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    ui_init();
    ui_update("Booting...");

    wifi_init_sta();

    ui_update("WiFi connecting...");

    xTaskCreate(
        sync_task,        // 関数
        "sync_task",      // 名前
        4096,             // スタックサイズ
        NULL,             // 引数
        5,                // 優先度
        NULL              // ハンドル不要
    );

}
