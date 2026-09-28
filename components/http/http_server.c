#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

#include "config.h"
#include "wifi.h"
#include "presence.h"
#include "ui.h"
#include "metrics.h"

#include "cJSON.h"

static const char *TAG = "http";

// 宛先Slaveノードの定義
typedef struct {
    const char *name;
    const char *url;
} target_slave_t;

// テスト用送信先リスト（PCの有線IPv6アドレスを指定）
// ※ PCの有線IPv6アドレスに合わせて書き換えてください
#define PC_IPV6 "2400:4151:7642:2a10:a5f8:7d29:742e:4922"

static const target_slave_t SLAVE_TARGETS[] = {
    {"Slave-Normal-1", "http://[" PC_IPV6 "]:8001/notify"},
    {"Slave-Normal-2", "http://[" PC_IPV6 "]:8002/notify"},
    {"Slave-Hang-1",   "http://[" PC_IPV6 "]:8004/notify"}, // 10秒ハング
    {"Slave-Normal-3", "http://[" PC_IPV6 "]:8003/notify"},
    {"Slave-Dead-1",   "http://[2400:4151:7642:2a10::9999]:80/notify"}, // 電源断 (未割り当てIP)
};
#define NUM_TARGETS (sizeof(SLAVE_TARGETS) / sizeof(SLAVE_TARGETS[0]))

// 送信実行タスク
static void outbound_notify_task(void *pvParameters) {
    ESP_LOGI("OUTBOUND", "=== Starting sequential notification to %d slaves ===", NUM_TARGETS);

    char post_data[] = "{\"event\":\"presence_update\",\"status\":\"active\"}";
    int64_t total_start = esp_timer_get_time();

    for (int i = 0; i < NUM_TARGETS; i++) {
        int64_t start_time = esp_timer_get_time();

        esp_http_client_config_t config = {
            .url = SLAVE_TARGETS[i].url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 4000, // タイムアウト4秒
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
        esp_http_client_set_header(client, "Content-Type", "application/json");

        ESP_LOGI("OUTBOUND", "[%d/%d] Sending to %s (%s)...", 
                 i + 1, NUM_TARGETS, SLAVE_TARGETS[i].name, SLAVE_TARGETS[i].url);

        esp_err_t err = esp_http_client_perform(client);
        uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - start_time) / 1000);

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI("OUTBOUND", " -> %s: SUCCESS (HTTP %d, Time: %lu ms)", 
                     SLAVE_TARGETS[i].name, status_code, duration_ms);
        } else {
            ESP_LOGE("OUTBOUND", " -> %s: FAILED (%s, Time: %lu ms)", 
                     SLAVE_TARGETS[i].name, esp_err_to_name(err), duration_ms);
        }

        esp_http_client_cleanup(client);
    }

    uint32_t total_sec = (uint32_t)((esp_timer_get_time() - total_start) / 1000000);
    ESP_LOGI("OUTBOUND", "=== Finished all notifications in %lu seconds ===", total_sec);
    vTaskDelete(NULL);
}

// テスト起動エンドポイント: GET /api/test/notify_all
static esp_err_t notify_all_trigger_handler(httpd_req_t *req) {
    // 送信処理でHTTPサービスタスク自体をブロックしないよう、別タスクで実行
    xTaskCreate(outbound_notify_task, "outbound_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL);
    
    const char *resp = "{\"status\":\"outbound notification test triggered\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void log_http_access(httpd_req_t *req)
{
    if(!req) return;

    char *method = "unknown";
    char ipstr[64] = "unknown";
    char *ipver = "unknown";
    int sockfd = httpd_req_to_sockfd(req);

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if(req->method == HTTP_GET)
    {
        method = "GET";
    }
    else if(req->method == HTTP_POST)
    {
        method = "POST";
    }

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        if (addr.ss_family == AF_INET)
        {
            struct sockaddr_in *a = (struct sockaddr_in *)&addr;
            inet_ntop(AF_INET, &a->sin_addr, ipstr, sizeof(ipstr));
            ipver = "IPv4";
        }
        else if (addr.ss_family == AF_INET6)
        {
            struct sockaddr_in6 *a = (struct sockaddr_in6 *)&addr;
            inet_ntop(AF_INET6, &a->sin6_addr, ipstr, sizeof(ipstr));
            ipver = "IPv6";
        }
    }

    ESP_LOGI(TAG, "%s %s from [%s]%s", method, req->uri, ipver, ipstr);

}

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

    log_http_access(req);
    return ESP_OK;
}

/* ===== GET /state ===== */
static esp_err_t state_get_handler(httpd_req_t *req)
{
    int uptime = esp_log_timestamp() / 1000;
    char resp[256];

    httpd_resp_set_type(req, "application/json");

    snprintf(resp,
         sizeof(resp),
         "{"
         "\"hostname\":\"%s\",",
         g_config.hostname);
    httpd_resp_send_chunk(req, resp, HTTPD_RESP_USE_STRLEN);

    snprintf(resp, sizeof(resp),
             "\"ipv4\":\"%s\","
             "\"ipv4_ready\":%d,"
             "\"ipv6\":\"%s\","
             "\"ipv6_ready\":%d,"
             "\"status\":%d,",
             g_ipv4_str,
             g_ipv4_ready,
             g_ipv6_str,
             g_ipv6_ready,
             g_presence.status
    );
    httpd_resp_send_chunk(req, resp, HTTPD_RESP_USE_STRLEN);

    snprintf(resp, sizeof(resp), "\"comment\":\"%s\",", g_presence.comment);
    httpd_resp_send_chunk(req, resp, HTTPD_RESP_USE_STRLEN);

    snprintf(resp, sizeof(resp),
             "\"rssi\":%d,"
             "\"uptime\":%d"
             "}",
             g_rssi,
             uptime
    );

    httpd_resp_send_chunk(req, resp, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    log_http_access(req);
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

    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *comment = cJSON_GetObjectItem(root, "comment");

    if (cJSON_IsNumber(status)) {
        g_presence.status = status->valueint;
    }

    if (cJSON_IsString(comment)) {
        strncpy(g_presence.comment, comment->valuestring, sizeof(g_presence.comment) - 1);
        g_presence.comment[sizeof(g_presence.comment) - 1] = '\0';
    }
    cJSON_Delete(root);

    httpd_resp_sendstr(req, "OK");

    ui_update(&g_presence);

    log_http_access(req);
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

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *comment = cJSON_GetObjectItem(root, "comment");

    if (cJSON_IsNumber(status)) {
        g_presence.status = status->valueint;
    }

    if (cJSON_IsString(comment)&& comment->valuestring) {
        strncpy(g_presence.comment, comment->valuestring, sizeof(g_presence.comment) - 1);
        g_presence.comment[sizeof(g_presence.comment) - 1] = '\0';
    }
    cJSON_Delete(root);

    httpd_resp_sendstr(req, "OK");

    ui_update(&g_presence);

    log_http_access(req);
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

        // http_server_start 内で以下を登録
        httpd_uri_t uri_test = {
            .uri      = "/api/test/notify_all",
            .method   = HTTP_GET,
            .handler  = notify_all_trigger_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_test);

        // メトリクス初期化
        metrics_init(server);

        ESP_LOGI(TAG, "HTTP server started");
    }
}