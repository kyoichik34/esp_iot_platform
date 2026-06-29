#include "presence.h"
#include "config.h"
#include "wifi.h"
#include "http_server.h"

#include "esp_log.h"
#include "esp_http_client.h"

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

/* ===== 全送信 ===== */
void presence_send_update_all(void)
{
    if (!g_config.send_update)
        return;

    shuffle(g_config.masters, g_config.master_count);

    for (int i = 0; i < g_config.master_count; i++) {
        send_one(g_config.masters[i]);
    }
}