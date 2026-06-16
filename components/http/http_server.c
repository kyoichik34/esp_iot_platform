#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "http";

/* ===== GET /state ===== */
static esp_err_t state_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /state");

    ui_update("/state");

    /* uptime（秒） */
    int uptime = esp_log_timestamp() / 1000;

    char resp[256];

    snprintf(resp, sizeof(resp),
             "{"
             "\"hostname\":\"%s\","
             "\"ipv4\":\"%s\","
             "\"ipv6\":\"%s\","
             "\"status\":\"online\","
             "\"rssi\":%d,"
             "\"uptime\":%d"
             "}",
             g_config.hostname,
             g_ipv4_str,
             g_ipv6_str,
             g_rssi,
             uptime
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* ===== サーバ起動 ===== */
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t uri = {
            .uri = "/state",
            .method = HTTP_GET,
            .handler = state_get_handler
        };

        httpd_register_uri_handler(server, &uri);

        ESP_LOGI(TAG, "HTTP server started");
    }
}