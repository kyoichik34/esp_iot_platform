#include "config.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "config";

config_t g_config;

/* ===== デフォルト設定 ===== */
static void set_default(void)
{
    memset(&g_config, 0, sizeof(g_config));

    /* 基本 */
    strcpy(g_config.hostname, "device1");

    /* WiFi（仮） */
    strcpy(g_config.wifi.ssid, "YOUR_SSID");
    strcpy(g_config.wifi.password, "YOUR_PASSWORD");

    /* presence */
    g_config.send_update = true;

    g_config.master_count = 1;
    strcpy(g_config.masters[0], "master.example.com");

    /* sync */
    g_config.sync_interval = 10;

    /* NTP */
    strcpy(g_config.ntp_server, "ntp.nict.jp");
    g_config.ntp_interval = 3600;

    /* DDNS */
    strcpy(g_config.ddns_target, "");
}

/* ===== JSON解析 ===== */
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
                sizeof(g_config.hostname) - 1);
    }

    /* ===== WiFi ===== */
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {

        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi, "password");

        if (cJSON_IsString(ssid)) {
            strncpy(g_config.wifi.ssid,
                    ssid->valuestring,
                    sizeof(g_config.wifi.ssid) - 1);
        }

        if (cJSON_IsString(pass)) {
            strncpy(g_config.wifi.password,
                    pass->valuestring,
                    sizeof(g_config.wifi.password) - 1);
        }
    }

    /* ===== send_update ===== */
    cJSON *send_update = cJSON_GetObjectItem(root, "send_update");
    if (cJSON_IsBool(send_update)) {
        g_config.send_update = cJSON_IsTrue(send_update);
    }

    /* ===== masters（配列） ===== */
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
                        sizeof(g_config.masters[i]) - 1);
            }
        }
    }

    /* ===== sync_interval ===== */
    cJSON *sync = cJSON_GetObjectItem(root, "sync_interval");
    if (cJSON_IsNumber(sync)) {
        g_config.sync_interval = sync->valueint;
    }

    /* ===== NTP ===== */
    cJSON *ntp = cJSON_GetObjectItem(root, "ntp");
    if (ntp) {

        cJSON *server = cJSON_GetObjectItem(ntp, "server");
        cJSON *interval = cJSON_GetObjectItem(ntp, "interval");

        if (cJSON_IsString(server)) {
            strncpy(g_config.ntp_server,
                    server->valuestring,
                    sizeof(g_config.ntp_server) - 1);
        }

        if (cJSON_IsNumber(interval)) {
            g_config.ntp_interval = interval->valueint;
        }
    }

    /* ===== DDNS ===== */
    cJSON *ddns = cJSON_GetObjectItem(root, "ddns_target");
    if (cJSON_IsString(ddns)) {
        strncpy(g_config.ddns_target,
                ddns->valuestring,
                sizeof(g_config.ddns_target) - 1);
    }

    cJSON_Delete(root);
}

/* ===== 初期化 ===== */
void config_init(void)
{
    set_default();

    FILE *f = fopen("/spiffs/config.json", "r");
    if (!f) {
        ESP_LOGW(TAG, "config.json not found → using default");
        return;
    }

    char buf[1024] = {0};
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (len > 0) {
        ESP_LOGI(TAG, "config.json loaded");
        load_json(buf);
    } else {
        ESP_LOGW(TAG, "config.json empty");
    }

    /* ===== ログ ===== */
    ESP_LOGI(TAG, "hostname: %s", g_config.hostname);
    ESP_LOGI(TAG, "send_update: %d", g_config.send_update);
    ESP_LOGI(TAG, "master_count: %d", g_config.master_count);

    for (int i = 0; i < g_config.master_count; i++) {
        ESP_LOGI(TAG, "master[%d]: %s", i, g_config.masters[i]);
    }

    ESP_LOGI(TAG, "SSID: %s", g_config.wifi.ssid);
    ESP_LOGI(TAG, "PASS: %s", g_config.wifi.password);
}