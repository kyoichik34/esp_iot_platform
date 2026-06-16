#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "ui.h"
#include "wifi.h"

#include "cJSON.h"   // ★追加

static const char *TAG = "http";

/* ===== 内部状態 ===== */
static char g_message[64] = "online";

/* ===== GET /state ===== */
static esp_err_t state_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /state");

    ui_update("/state");

    int uptime = esp_log_timestamp() / 1000;

    char resp[512];

    snprintf(resp, sizeof(resp),
             "{"
             "\"hostname\":\"%s\","
             "\"ipv4\":\"%s\","
             "\"ipv6\":\"%s\","
             "\"status\":\"%s\","
             "\"rssi\":%d,"
             "\"uptime\":%d"
             "}",
             g_config.hostname,
             g_ipv4_str,
             g_ipv6_str,
             g_message,
             g_rssi,
             uptime
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* ===== POST /state ===== */
static esp_err_t state_post_handler(httpd_req_t *req)
{
    char buf[128];

    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) {
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "No data");
        return ESP_FAIL;
    }

    buf[len] = 0;

    ESP_LOGI(TAG, "POST /state: %s", buf);

    /* JSON解析 */
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "message");

    if (cJSON_IsString(msg) && msg->valuestring) {

        strncpy(g_message,
                msg->valuestring,
                sizeof(g_message));

        g_message[sizeof(g_message)-1] = '\0';

        ui_update(g_message);

        ESP_LOGI(TAG, "Message updated: %s", g_message);
    }

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

/* ===== サーバ起動 ===== */
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t get_uri = {
            .uri = "/state",
            .method = HTTP_GET,
            .handler = state_get_handler
        };

        httpd_uri_t post_uri = {
            .uri = "/state",
            .method = HTTP_POST,
            .handler = state_post_handler
        };

        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);

        ESP_LOGI(TAG, "HTTP server started");
    }
}
