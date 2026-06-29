#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 定数 ===== */
#define MAX_MASTERS 5

/* ===== WiFi設定 ===== */
/* ※ wifi_config_t と名前衝突しないように注意 */
typedef struct {
    char ssid[32];
    char password[64];
} wifi_cred_t;

/* ===== 全体設定 ===== */
typedef struct {

    /* ===== 基本 ===== */
    char hostname[32];     // ノード識別名

    /* ===== WiFi ===== */
    wifi_cred_t wifi;

    /* ===== Presence ===== */
    bool send_update;      // sendするか
    int  master_count;     // master数
    char masters[MAX_MASTERS][64];  // ドメイン or IP

    int is_slave;

    int  sync_interval;    // 秒（heartbeat）

    /* ===== NTP ===== */
    char ntp_server[64];
    int  ntp_interval;     // 秒

    /* ===== DDNS ===== */
    char ddns_target[64];  // ping先

} config_t;

/* ===== グローバル ===== */
extern config_t g_config;
extern bool config_valid;

/* ===== API ===== */
void config_init(void);

#ifdef __cplusplus
}
#endif
