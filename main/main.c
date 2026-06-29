#include <stdio.h>
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_http_client.h"

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

void presence_send_update_all(void)
{
    char json[256];

    const char* status = ui_get_status();
    snprintf(json, sizeof(json),
        "{ \"hostname\":\"%s\", \"status\":\"%s\" }",
        g_config.hostname,
        status
    );

    for (int i = 0; i < g_config.master_count; i++) {
        char url[128];
        snprintf(url, sizeof(url),
                 "http://%s/update",
                 g_config.masters[i]);
        ESP_LOGI("presence", "POST %s", url);
        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_POST,
        };
        esp_http_client_handle_t client =
            esp_http_client_init(&config);
        esp_http_client_set_header(client,
            "Content-Type", "application/json");
        esp_http_client_set_post_field(client,
            json, strlen(json));
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI("presence", "POST OK");
        } else {
            ESP_LOGE("presence", "POST FAIL");
        }
        esp_http_client_cleanup(client);
    }
}

void presence_poll_master(void)
{
    for (int i = 0; i < g_config.master_count; i++) {

        char url[128];
        snprintf(url, sizeof(url),
                 "http://%s/devices",
                 g_config.masters[i]);
        ESP_LOGI("presence", "GET %s", url);
        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_GET,
        };
        esp_http_client_handle_t client =
            esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int len = esp_http_client_get_content_length(client);
            char buf[512] = {0};
            esp_http_client_read(client, buf, sizeof(buf)-1);
            ESP_LOGI("presence", "RECV: %s", buf);
            /* TODO: JSON parseしてdevice table更新 */
        } else {
            ESP_LOGE("presence", "GET FAIL");
        }
        esp_http_client_cleanup(client);
    }
}

void sync_task(void *arg)
{
    while (1) {

        vTaskDelay(g_config.sync_interval * 1000 / portTICK_PERIOD_MS);

        if (g_config.is_slave) {
            presence_poll_master();     // ★pull
        } else {
            presence_send_update_all(); // ★heartbeat
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
