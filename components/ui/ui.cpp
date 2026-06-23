#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include "wifi.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ui";

static LGFX lcd;

/* ===== 外部状態 ===== */
extern char g_ipv4_str[32];
extern char g_ipv6_str[64];
extern int  g_ipv4_ready;
extern int  g_ipv6_ready;

/* ===== LCD抽象（暫定：ログ出力にする） ===== */

static void lcd_clear(void)
{
    ESP_LOGI(TAG, "LCD CLEAR");
    lcd.fillScreen(TFT_BLACK);
}

static void lcd_draw_string(int x, int y, const char *str)
{
    ESP_LOGI(TAG, "LCD[%d,%d]: %s", x, y, str);

    lcd.setCursor(x, y);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.print(str);
}

/* ===== 初期化 ===== */

void ui_init(void)
{
    lcd.init();
    lcd.setRotation(1);

    lcd_clear();

    lcd_draw_string(0, 0, "ESP32 Presence");

    lcd_draw_string(0, 16, "Starting...");
}

/* ===== メッセージ表示 ===== */
void ui_update(const char *msg)
{
    ESP_LOGI(TAG, "ui_update: %s", msg);

    lcd_clear();

    char buf[64];

    snprintf(buf, sizeof(buf),
             "MSG: %.32s", msg);

    lcd_draw_string(0, 0, buf);
}

/* ===== ネットワーク表示 ===== */
void ui_update_network(void)
{
    ESP_LOGI(TAG, "ui_update_network");

    lcd_clear();
    lcd.setCursor(0, 0);
    lcd.setTextColor(TFT_WHITE);
    lcd.fillScreen(TFT_BLACK);


    char line1[64];
    char line2[64];

    /* IPv4 */
    snprintf(line1, sizeof(line1),
             "IPv4 %c %s",
             g_ipv4_ready ? 'O' : 'X',
             g_ipv4_ready ? g_ipv4_str : "-");

    /* IPv6 */
    snprintf(line2, sizeof(line2),
             "IPv6 %c %.20s",
             g_ipv6_ready ? 'O' : 'X',
             g_ipv6_ready ? g_ipv6_str : "-");

    lcd_draw_string(0, 0, line1);
    lcd_draw_string(0, 16, line2);
}