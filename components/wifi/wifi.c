#include "wifi.h"
#include "config.h"
#include "ui.h"
#include "http_server.h"

#include <string.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"

#include "lwip/inet.h"
#include "ping/ping_sock.h"

static const char *TAG = "wifi";

static esp_netif_t* s_netif = NULL;

/* ===== 状態共有 ===== */
char g_ipv4_str[32] = "0.0.0.0";
char g_ipv6_str[64] = "N/A";
int  g_rssi = 0;

/* ===== DDNS ping ===== */
static void start_ping(void)
{
    if (strlen(g_config.ddns_target) == 0) {
        return;
    }

    ip_addr_t addr;

    if (!ipaddr_aton(g_config.ddns_target, &addr)) {
        ESP_LOGE(TAG, "Invalid IPv6");
        return;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = addr;
    config.count = 1;

    esp_ping_callbacks_t cbs = {0};

    esp_ping_handle_t ping;
    esp_ping_new_session(&config, &cbs, &ping);
    esp_ping_start(ping);

    ESP_LOGI(TAG, "Ping: %s", g_config.ddns_target);
}

/* ===== イベント ===== */
static void handler(void* arg,
                    esp_event_base_t base,
                    int32_t id,
                    void* data)
{
    if (base == WIFI_EVENT) {

        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        }

        if (id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "Connected");
            esp_netif_create_ip6_linklocal(s_netif);
        }

        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "Disconnected → retry");
            esp_wifi_connect();
        }
    }

    /* ===== IPv4 ===== */
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* e = data;

        snprintf(g_ipv4_str, sizeof(g_ipv4_str),
                 IPSTR,
                 IP2STR(&e->ip_info.ip));

        ESP_LOGI(TAG, "IPv4: %s", g_ipv4_str);

        /* RSSI取得 */
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            g_rssi = ap.rssi;
        }

        static int http_started = 0;
        if (!http_started) {
            http_server_start();
            http_started = 1;
        }
    }

    /* ===== IPv6 ===== */
    if (base == IP_EVENT && id == IP_EVENT_GOT_IP6) {

        ip_event_got_ip6_t* e = data;

        esp_ip6_addr_type_t type =
            esp_netif_ip6_get_addr_type(&e->ip6_info.ip);

        ESP_LOGI(TAG, "IPv6: " IPV6STR " (%d)",
                 IPV62STR(e->ip6_info.ip), type);

        if (type == ESP_IP6_ADDR_IS_GLOBAL) {

            snprintf(g_ipv6_str, sizeof(g_ipv6_str),
                     IPV6STR,
                     IPV62STR(e->ip6_info.ip));

            ui_update(g_ipv6_str);

            start_ping();
        }
    }
}

/* ===== 初期化 ===== */
void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &handler, NULL, NULL);

    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &handler, NULL, NULL);

    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_GOT_IP6,
        &handler, NULL, NULL);

    wifi_config_t wifi_cfg = {0};

    strcpy((char*)wifi_cfg.sta.ssid,
           g_config.wifi.ssid);
    strcpy((char*)wifi_cfg.sta.password,
           g_config.wifi.password);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_wifi_start();
}