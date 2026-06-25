#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include "wifi.h"
#include "config.h"

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

static char g_ui_status[32] = "READY";

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

static void draw_status(void)
{
    int width  = lcd.width();
    int height = lcd.height();

    char line1[96];
    char line2[96];
    char line3[96];

    snprintf(line1, sizeof(line1), "%s", g_config.hostname);

    snprintf(line2, sizeof(line2),
             "v4:%s",
             g_ipv4_ready ? g_ipv4_str : "-");

    snprintf(line3, sizeof(line3),
             "v6:%s",
             g_ipv6_ready ? g_ipv6_str : "-");

    /* フォントサイズ考慮してY位置計算 */
    const float textsize = 1.8;
    int line_h = 16 * textsize;  // textSize=2想定
    int y3 = height - line_h;
    int y2 = y3 - line_h;
    int y1 = y2 - line_h;

    /* 右揃え */
    lcd.setTextSize(textsize);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);

    int x;

    x = width - lcd.textWidth(line1);
    lcd.setCursor(x, y1);
    lcd.print(line1);

    x = width - lcd.textWidth(line2);
    lcd.setCursor(x, y2);
    lcd.print(line2);

    x = width - lcd.textWidth(line3);
    lcd.setCursor(x, y3);
    lcd.print(line3);
}

static void draw_center_text(const char *text)
{
    int screen_w = lcd.width();
    int screen_h = lcd.height();

    lcd.setTextSize(3);   // ← デカくする
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    int text_w = lcd.textWidth(text);
    int text_h = 16 * 3;

    int x = (screen_w - text_w) / 2;
    int y = (screen_h - text_h) / 2;

    lcd.setCursor(x, y);
    lcd.print(text);
}

/* ===== 初期化 ===== */
void ui_init(void)
{
    lcd.init();
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setRotation(1);
    lcd.setTextSize(2);

    lcd_clear();

    draw_center_text("BOOT");

    lcd.setCursor(0, 0);
    lcd.print("ESP32 Presence");

    draw_status();   // ★ここでも出す
}

/* ===== メッセージ表示 ===== */
void ui_update(const char *msg)
{
    ESP_LOGI(TAG, "ui_update: %s", msg);

    strncpy(g_ui_status, msg, sizeof(g_ui_status)-1);

    lcd_clear();

    draw_center_text(g_ui_status); // 中央表示

    draw_status();          // ← 右下固定
}
/* ===== ネットワーク表示 ===== */
void ui_update_network(void)
{
    lcd_clear();

    lcd.setTextSize(2);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    draw_status();   // ★追加
}
