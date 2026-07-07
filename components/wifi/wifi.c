#include <string.h>
#include <stdio.h>

#include "wifi.h"
#include "config.h"
#include "ui.h"
#include "http_server.h"
#include "presence.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "lwip/inet.h"

static const char *TAG = "wifi";

/* ===== 状態 ===== */
char g_ipv4_str[32] = "0.0.0.0";
char g_ipv6_str[64] = "N/A";

int g_ipv4_ready = 0;
int g_ipv6_ready = 0;
int g_rssi = 0;

static char g_last_global_ipv6[64] = "";

static esp_netif_t* s_netif = NULL;


static void ddns_update(void *arg)
{
    char url[256];

    if (strlen(g_config.ddns_host_key) == 0) {
        ESP_LOGW(TAG, "DDNS host key not configured");
        return;
    }
    /* 東日本URL/西日本プロキシURL切替 */
    const char *base_url = g_config.ddns_use_west_proxy ?
            g_config.ddns_url_west : g_config.ddns_url_east;

    snprintf(url, sizeof(url), "%s?%s", base_url, g_config.ddns_host_key);

    ESP_LOGI(TAG, "DDNS UPDATE: %s", url);


   esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "DDNS status=%d", status);
    }
    else {
        ESP_LOGE(TAG, "DDNS update failed");
    }

    esp_http_client_cleanup(client);

    /* task関数なので必ずDeleteすること */
    vTaskDelete(NULL);
}

static void handler(void* arg,
                    esp_event_base_t base,
                    int32_t id,
                    void* data)
{
    static int ready_sent = 0;
    ESP_LOGI(TAG, "EVENT base=%s id=%ld", base, id);

    if (base == WIFI_EVENT) {

        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            esp_wifi_connect();
        }

        if (id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            esp_netif_create_ip6_linklocal(s_netif);
        }

        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");

            wifi_event_sta_disconnected_t* e = data;
            ESP_LOGW(TAG, "disconnect reason=%d", e->reason);

            ready_sent = 0;
            ui_update("Disconnected");

            esp_wifi_connect();
        }
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {

        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");

        ip_event_got_ip_t* e = data;

        snprintf(g_ipv4_str, sizeof(g_ipv4_str),
                 IPSTR,
                 IP2STR(&e->ip_info.ip));

        g_ipv4_ready = 1;

        ESP_LOGI(TAG, "IPv4: %s", g_ipv4_str);

        /* 初回のみ */
        if (!ready_sent) {
            ui_update("READY");
            ready_sent = 1;
        }

        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            g_rssi = ap.rssi;
            ESP_LOGI(TAG, "RSSI: %d", g_rssi);
        }

        static int started = 0;
        if (!started) {
            http_server_start();
            started = 1;
        }
    }

    if (base == IP_EVENT && id == IP_EVENT_GOT_IP6) {

        ESP_LOGI(TAG, "IP_EVENT_GOT_IP6");

        ip_event_got_ip6_t* e = data;

        esp_ip6_addr_type_t type =
            esp_netif_ip6_get_addr_type(&e->ip6_info.ip);

        ESP_LOGI(TAG, "IPv6: " IPV6STR " (%d)",
                 IPV62STR(e->ip6_info.ip), type);

        if (type == ESP_IP6_ADDR_IS_GLOBAL) {
            strncpy(g_last_global_ipv6, g_ipv6_str, sizeof(g_last_global_ipv6));
            g_last_global_ipv6[sizeof(g_last_global_ipv6) - 1] = '\0';

            snprintf(g_ipv6_str, sizeof(g_ipv6_str), IPV6STR, IPV62STR(e->ip6_info.ip));

            g_ipv6_ready = 1;

            ESP_LOGI(TAG, "IPv6 READY");
            if (!ready_sent) {
                ui_update("READY");
                ready_sent = 1;
            }

            if (strcmp(g_last_global_ipv6, g_ipv6_str) == 0) {
                ESP_LOGI(TAG, "IPv6 unchanged. Skip DDNS update.");
                return;
            }

            ESP_LOGI(TAG, "IPv6 changed: %s -> %s DDNS update required.", g_last_global_ipv6, g_ipv6_str);

            /* ブロックしない */
            xTaskCreate(ddns_update, "ddns_update_task", 4096, NULL, 5, NULL);

            ui_update_network();
        }
    }
}

void wifi_init_sta(void)
{
    ESP_LOGI(TAG, "wifi_init_sta called");

	ESP_LOGI(TAG, "SSID=%s", g_config.wifi.ssid);
	ESP_LOGI(TAG, "PASS=%s", g_config.wifi.password);

    esp_netif_init();
	esp_err_t err = esp_event_loop_create_default();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
	    ESP_LOGE(TAG, "event loop failed");
	}
    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

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

    strcpy((char*)wifi_cfg.sta.ssid, g_config.wifi.ssid);
    strcpy((char*)wifi_cfg.sta.password, g_config.wifi.password);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_set_ps(WIFI_PS_NONE);

	ESP_LOGI(TAG, "wifi start");
	esp_wifi_start();

	ESP_LOGI(TAG, "wifi connect");
	esp_wifi_connect();
}