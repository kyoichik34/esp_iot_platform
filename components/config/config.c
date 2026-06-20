#include "config.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "config";

config_t g_config;

/* ===== デフォルト ===== */
static void set_default(void)
{
    memset(&g_config, 0, sizeof(g_config));

    strcpy(g_config.hostname, "device1");

    /* WiFi（仮） */
    strcpy(g_config.wifi.ssid, "YOUR_SSID");
    strcpy(g_config.wifi.password, "YOUR_PASSWORD");

    /* presence */
    g_config.send_update = true;

    g_config.master_count = 1;
    strcpy(g_config.masters[0], "master.example.com");
}

/* ===== JSON読み込み（SPIFFSなどから読む想定） ===== */
static void load_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed → use default");
        return;
    }

    /* hostname */
    cJSON *hostname = cJSON_GetObjectItem(root, "hostname");
    if (cJSON_IsString(hostname)) {
        strncpy(g_config.hostname,
                hostname->valuestring,
                sizeof(g_config.hostname));
    }

    /* WiFi */
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {

        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi, "password");

        if (cJSON_IsString(ssid)) {
            strncpy(g_config.wifi.ssid,
                    ssid->valuestring,
                    sizeof(g_config.wifi.ssid));
        }

        if (cJSON_IsString(pass)) {
            strncpy(g_config.wifi.password,
                    pass->valuestring,
                    sizeof(g_config.wifi.password));
        }
    }

    /* send_update */
    cJSON *send_update = cJSON_GetObjectItem(root, "send_update");
    if (cJSON_IsBool(send_update)) {
        g_config.send_update = cJSON_IsTrue(send_update);
    }

    /* masters（配列） */
    cJSON *masters = cJSON_GetObjectItem(root, "masters");
    if (cJSON_IsArray(masters)) {

        int count = cJSON_GetArraySize(masters);

        if (count > MAX_MASTERS)
            count = MAX_MASTERS;

        g_config.master_count = count;

        for (int i = 0; i < count; i++) {

            cJSON *m = cJSON_GetArrayItem(masters, i);

            if (cJSON_IsString(m)) {
                strncpy(g_config.masters[i],
                        m->valuestring,
                        sizeof(g_config.masters[i]));
            }
        }
    }

    cJSON_Delete(root);
}

/* ===== 初期化 ===== */
void config_init(void)
{
    set_default();

    /* ===== 本来はファイルから読む ===== */
    /* 例：SPIFFS / LittleFS など */

    /* 仮テスト用JSON */
    const char *test_json =
        "{"
        "\"hostname\":\"device1\","
        "\"send_update\":true,"
        "\"masters\":["
        "\"master1.example.com\","
        "\"master2.example.com\""
        "],"
        "\"wifi\":{"
        "\"ssid\":\"YOUR_SSID\","
        "\"password\":\"YOUR_PASSWORD\""
        "}"
        "}";

    load_json(test_json);

    /* ===== ログ ===== */
    ESP_LOGI(TAG, "hostname: %s", g_config.hostname);
    ESP_LOGI(TAG, "send_update: %d", g_config.send_update);
    ESP_LOGI(TAG, "master_count: %d", g_config.master_count);

    for (int i = 0; i < g_config.master_count; i++) {
        ESP_LOGI(TAG, "master[%d]: %s", i, g_config.masters[i]);
    }
}