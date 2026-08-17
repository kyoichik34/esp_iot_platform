#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

#include "config.h"
#include "wifi.h"
#include "presence.h"
#include "ui.h"

#include "cJSON.h"

static const char *TAG = "http";


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


        ESP_LOGI(TAG, "HTTP server started");
    }
}