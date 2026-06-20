#pragma once

#include <stdbool.h>

#define MAX_MASTERS 5

/* ★名前変更 */
typedef struct {
    char ssid[32];
    char password[64];
} wifi_cred_t;   // ←変更

typedef struct {

    wifi_cred_t wifi;   // ←ここも変更

    char hostname[32];

    bool send_update;
    int  master_count;
    char masters[MAX_MASTERS][64];

} config_t;

extern config_t g_config;

void config_init(void);
