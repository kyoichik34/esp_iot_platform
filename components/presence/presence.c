#include "presence.h"
#include "config.h"
#include "wifi.h"
#include "http_server.h"
#include "presence.h"
#include "ui.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "presence";

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

/* ===== 送信 ===== */
static void send_one(const char *host)
{
    char url[128];
    snprintf(url, sizeof(url),
             "http://%s/update", host);
    char body[128];
    snprintf(body, sizeof(body),
             "{\"hostname\":\"%s\",\"status\":\"%s\"}",
             g_config.hostname,
             g_message);
    ESP_LOGI(TAG, "send -> %s", host);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 1000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
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

            if (cJSON_IsString(status) && status->valuestring) {

                /* ★ 同じ状態なら更新しない（無駄防止） */
                if (strcmp(ui_get_status(), status->valuestring) != 0) {

                    strncpy(g_message, status->valuestring, sizeof(g_message));
                    g_message[sizeof(g_message) - 1] = '\0';

                    ui_update(status->valuestring);
                }
            }

            cJSON_Delete(root);
        }
    }

    return ESP_OK;
}

/* ===== 全送信 ===== */
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


