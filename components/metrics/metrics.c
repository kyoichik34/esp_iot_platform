#include "metrics.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "METRICS";
static httpd_handle_t s_server = NULL;
static metrics_stats_t s_stats = {0};

void metrics_record_request(bool success, uint32_t latency_us) {
    s_stats.total_requests++;
    if (success) {
        s_stats.success_requests++;
    } else {
        s_stats.error_requests++;
    }
    s_stats.last_latency_us = latency_us;
}

metrics_stats_t metrics_get_stats(void) {
    return s_stats;
}

// 現在のアクティブソケット数を取得
static size_t get_active_socket_count(void) {
    if (s_server == NULL) {
        return 0;
    }
    size_t num_clients = 0;
    if (httpd_get_client_list(s_server, &num_clients, NULL) == ESP_OK) {
        return num_clients;
    }
    return 0;
}

// -------------------------------------------------------------
// HTTPハンドラ: GET /api/metrics
// -------------------------------------------------------------
static esp_err_t metrics_get_handler(httpd_req_t *req) {
    int64_t start_time = esp_timer_get_time();

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 稼働時間 (秒)
    cJSON_AddNumberToObject(root, "uptime_sec", (double)(esp_timer_get_time() / 1000000));

    // メモリ情報 (バイト単位)
    cJSON_AddNumberToObject(root, "int_sram_free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(root, "int_sram_min", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(root, "psram_free", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(root, "psram_min", heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

    // ソケット・負荷統計
    cJSON_AddNumberToObject(root, "active_sockets", (double)get_active_socket_count());
    cJSON_AddNumberToObject(root, "req_total", s_stats.total_requests);
    cJSON_AddNumberToObject(root, "req_success", s_stats.success_requests);
    cJSON_AddNumberToObject(root, "req_error", s_stats.error_requests);
    cJSON_AddNumberToObject(root, "last_latency_us", s_stats.last_latency_us);

    const char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    cJSON_free((void *)json_str);
    cJSON_Delete(root);

    // /api/metrics 自体の処理時間も記録
    uint32_t duration = (uint32_t)(esp_timer_get_time() - start_time);
    metrics_record_request(true, duration);

    return ESP_OK;
}

// -------------------------------------------------------------
// シリアルポート定期出力タスク (5秒周期)
// -------------------------------------------------------------
static void metrics_logging_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t int_min  = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t sockets = get_active_socket_count();
        uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000);

        ESP_LOGI(TAG, "[STAT] Up:%lus | IntSRAM:%lu B (Min:%lu B) | PSRAM:%lu B | Sockets:%d | Req(Tot/OK/Err):%lu/%lu/%lu",
                 uptime,
                 int_free,
                 int_min,
                 psram_free,
                 sockets,
                 s_stats.total_requests,
                 s_stats.success_requests,
                 s_stats.error_requests);
    }
}

// -------------------------------------------------------------
// 初期化・登録
// -------------------------------------------------------------
esp_err_t metrics_init(httpd_handle_t server) {
    if (server == NULL) {
        ESP_LOGE(TAG, "Server handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    s_server = server;

    // URIハンドラ登録
    httpd_uri_t uri_metrics = {
        .uri      = "/api/metrics",
        .method   = HTTP_GET,
        .handler  = metrics_get_handler,
        .user_ctx = NULL
    };
    esp_err_t err = httpd_register_uri_handler(server, &uri_metrics);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/metrics URI: %s", esp_err_to_name(err));
        return err;
    }

    // シリアル定期出力タスクの起動 (内部SRAMを圧迫しないようスタックは2.5KB)
    BaseType_t ret = xTaskCreate(metrics_logging_task, "metrics_task", 2560, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create metrics_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Metrics module initialized successfully");
    return ESP_OK;
}
