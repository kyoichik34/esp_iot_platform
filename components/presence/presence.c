#include "presence.h"
#include "config.h"
#include "wifi.h"
#include "http_server.h"
#include "presence.h"
#include "ui.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "presence";

static int64_t g_last_send_time = 0;

presence_state_t g_presence = {
    .status = "READY",
    .comment = ""
};

/* ===== shuffle ===== */
static void shuffle(char masters[][64], int count)
{
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);

        char tmp[64];
        strcpy(tmp, masters[i]);
        strcpy(masters[i], masters[j]);
        strcpy(masters[j], tmp);
    }
}

/* ===== HTTP受信ハンドラ ===== */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {

        char buf[256] = {0};

        int len = evt->data_len;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;

        memcpy(buf, evt->data, len);
        buf[len] = '\0';

        ESP_LOGI(TAG, "RECV: %s", buf);

        /* ===== JSON parse ===== */
        cJSON *root = cJSON_Parse(buf);
        if (root) {

            cJSON *status = cJSON_GetObjectItem(root, "status");
            cJSON *comment = cJSON_GetObjectItem(root, "comment");

            if (cJSON_IsString(status) && status->valuestring) {
                if (strcmp(ui_get_status(), status->valuestring) != 0) {
                    strncpy(g_presence.status, status->valuestring, sizeof(g_presence.status) - 1);
                    g_presence.status[sizeof(g_presence.status) - 1] = '\0';
                }
            }

            if (cJSON_IsString(comment) && comment->valuestring) {
                strncpy(g_presence.comment, comment->valuestring, sizeof(g_presence.comment) - 1);
            }

            ui_update(g_presence.status);

            cJSON_Delete(root);
        }
    }

    return ESP_OK;
}

/* ===== 全送信 ===== */
void presence_send_update_all_with()
{
    char json[512];

    int64_t now = esp_timer_get_time();

    // 500ms以内は捨てる
    if (now - g_last_send_time < 500000) {
        ESP_LOGW(TAG, "send throttled");
        return;
    }

    g_last_send_time = now;

    snprintf(json, sizeof(json),
        "{"
        "\"hostname\":\"%s\","
        "\"status\":\"%s\","
        "\"comment\":\"%s\""
        "}",
        g_config.hostname,
        g_presence.status,
        g_presence.comment
    );

    for (int i = 0; i < g_config.master_count; i++) {
        char url[128];
        snprintf(url, sizeof(url),
                 "http://%s/update",
                 g_config.masters[i]);
        ESP_LOGI(TAG, "POST %s payload = %s", url, json);
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
            ESP_LOGI(TAG, "POST OK");
        } else {
            ESP_LOGE(TAG, "POST FAIL");
        }
        esp_http_client_cleanup(client);
    }
}

void presence_send_update_all(void)
{
    presence_send_update_all_with();
}

void presence_poll_master(void)
{
    for (int i = 0; i < g_config.master_count; i++) {

        char url[128];
        snprintf(url, sizeof(url),
                 "http://%s/state",
                 g_config.masters[i]);

        ESP_LOGI(TAG, "GET %s", url);

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 5000,
            .event_handler = http_event_handler,   // ★重要
        };

        esp_http_client_handle_t client =
            esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "retry...");
            err = esp_http_client_perform(client);
        }
        else if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP status=%d", status);
        }

        esp_http_client_cleanup(client);
    }
}


