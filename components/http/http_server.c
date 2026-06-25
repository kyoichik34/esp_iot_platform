#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "ui.h"
#include "wifi.h"
#include "presence.h"

#include "cJSON.h"

static const char *TAG = "http";

/* ===== 状態 ===== */
char g_message[64] = "online";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static esp_err_t root_handler(httpd_req_t *req)
{
    FILE *f = fopen("/spiffs/index.html", "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");

    char buf[256];
    size_t read_bytes;

    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(f);

    /* 終端 */
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

/* ===== GET /state ===== */
static esp_err_t state_get_handler(httpd_req_t *req)
{
    int uptime = esp_log_timestamp() / 1000;

    char resp[512];

    snprintf(resp, sizeof(resp),
             "{"
             "\"hostname\":\"%s\","
             "\"ipv4\":\"%s\","
             "\"ipv4_ready\":%d,"
             "\"ipv6\":\"%s\","
             "\"ipv6_ready\":%d,"
             "\"status\":\"%s\","
             "\"rssi\":%d,"
             "\"uptime\":%d"
             "}",
             g_config.hostname,
             g_ipv4_str,
             g_ipv4_ready,
             g_ipv6_str,
             g_ipv6_ready,
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
    if (len <= 0) return ESP_FAIL;

    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *msg = cJSON_GetObjectItem(root, "message");

    if (cJSON_IsString(msg) && msg->valuestring) {

        strncpy(g_message, msg->valuestring, sizeof(g_message));
        g_message[sizeof(g_message)-1] = '\0';

        ui_update(g_message);

        presence_send_update_all();   // ★ここ重要
    }

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

/* ===== POST /update ===== */
static esp_err_t update_handler(httpd_req_t *req)
{
    char buf[128];

    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;

    buf[len] = 0;

    ESP_LOGI(TAG, "UPDATE %s", buf);

    // 今はログだけ（後でdevices管理に拡張）

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

/* ===== サーバ ===== */
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

        httpd_uri_t update_uri = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = update_handler
        };

        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };

        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);
        httpd_register_uri_handler(server, &update_uri);

        httpd_register_uri_handler(server, &root);


        ESP_LOGI(TAG, "HTTP server started");
    }
}