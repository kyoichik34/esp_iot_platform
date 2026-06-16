#pragma once

typedef struct {
    char ssid[32];
    char password[64];
} app_wifi_config_t;

typedef struct {
    char server[64];
    int interval;
} ntp_config_t;

typedef struct {
    char mode[16];
    char hostname[64];
    char master_host[64];

    app_wifi_config_t wifi;

    int sync_interval;

    ntp_config_t ntp;

    char ddns_target[64];

} config_t;

extern config_t g_config;

void config_init(void);
int config_load(void);