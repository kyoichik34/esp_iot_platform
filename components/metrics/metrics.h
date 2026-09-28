#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// リクエスト処理統計
typedef struct {
    uint32_t total_requests;
    uint32_t success_requests;
    uint32_t error_requests;
    uint32_t last_latency_us; // 直近処理時間（マイクロ秒）
} metrics_stats_t;

/**
 * @brief メトリクスモジュールの初期化
 *        ・GET /api/metrics エンドポイントの登録
 *        ・シリアル定期出力バックグラウンドタスクの起動
 * 
 * @param server 有効な HTTP サーバーハンドル
 * @return esp_err_t ESP_OK: 成功
 */
esp_err_t metrics_init(httpd_handle_t server);

/**
 * @brief 各種HTTPハンドラ内でリクエスト処理実績を記録する
 * 
 * @param success 成功(true) / エラー(false)
 * @param latency_us 処理所要時間（マイクロ秒）
 */
void metrics_record_request(bool success, uint32_t latency_us);

/**
 * @brief 現在の統計情報を取得する（必要に応じて利用）
 */
metrics_stats_t metrics_get_stats(void);

#ifdef __cplusplus
}
#endif