#include "config.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "config";

config_t g_config;

void config_init(void)
{
    memset(&g_config, 0, sizeof(g_config));

    strcpy(g_config.mode, " standalone ");
    strcpy(g_config.hostname, "esp32");

    g_config.sync_interval = 10;

    strcpy(g_config.ntp.server, "ntp.nict.jp");
    g_config.ntp.interval = 3600;

    g_config.ddns_target[0] = '\0';
}

int config_load(void)
{
    FILE *f = fopen("/spiffs/config.json", "r");
    if (!f) {
        ESP_LOGE(TAG, "config.json open failed");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "JSON parse error");
        return -1;
    }

    cJSON *hostname = cJSON_GetObjectItem(root, "hostname");
    if (cJSON_IsString(hostname)) {
        strncpy(g_config.hostname,
                hostname->valuestring,
                sizeof(g_config.hostname));
        g_config.hostname[sizeof(g_config.hostname)-1] = '\0';
    }

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *password = cJSON_GetObjectItem(wifi, "password");

        if (cJSON_IsString(ssid)) {
            strncpy(g_config.wifi.ssid,
                    ssid->valuestring,
                    sizeof(g_config.wifi.ssid));
            g_config.wifi.ssid[sizeof(g_config.wifi.ssid)-1] = '\0';
        }

        if (cJSON_IsString(password)) {
            strncpy(g_config.wifi.password,
                    password->valuestring,
                    sizeof(g_config.wifi.password));
            g_config.wifi.password[sizeof(g_config.wifi.password)-1] = '\0';
        }
    }

    cJSON *ddns = cJSON_GetObjectItem(root, "ddns_target");
    if (cJSON_IsString(ddns)) {
        strncpy(g_config.ddns_target,
                ddns->valuestring,
                sizeof(g_config.ddns_target));
        g_config.ddns_target[sizeof(g_config.ddns_target)-1] = '\0';
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Config loaded: %s", g_config.hostname);
    ESP_LOGI(TAG, "SSID: %s", g_config.wifi.ssid);
    ESP_LOGI(TAG, "DDNS target: %s", g_config.ddns_target);

    return 0;
}
