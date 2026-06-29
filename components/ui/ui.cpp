#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include "wifi.h"
#include "config.h"
#include "presence.h"
#include "http_server.h"

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

static void draw_network(void)
{
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    char line1[96];
    char line2[96];

    snprintf(line1, sizeof(line1),
             "IPv4 %c %s",
             g_ipv4_ready ? 'O' : 'X',
             g_ipv4_ready ? g_ipv4_str : "-");

    snprintf(line2, sizeof(line2),
             "IPv6 %c %.30s",
             g_ipv6_ready ? 'O' : 'X',
             g_ipv6_ready ? g_ipv6_str : "-");

    lcd.setCursor(0, 0);
    lcd.print(line1);

    lcd.setCursor(0, 32);
    lcd.print(line2);
}

static void draw_buttons(void)
{
    int w = lcd.width();
    int h = lcd.height();

    int btn_w = w / 3;
    int btn_h = 40;
    int y = 0;

    lcd.setTextSize(2);

    // online
    lcd.fillRect(0, y, btn_w, btn_h, TFT_GREEN);
    lcd.setCursor(10, y + 10);
    lcd.setTextColor(TFT_BLACK);
    lcd.print("ONLINE");

    // away
    lcd.fillRect(btn_w, y, btn_w, btn_h, TFT_YELLOW);
    lcd.setCursor(btn_w + 10, y + 10);
    lcd.print("AWAY");

    // busy
    lcd.fillRect(btn_w * 2, y, btn_w, btn_h, TFT_RED);
    lcd.setCursor(btn_w * 2 + 10, y + 10);
    lcd.print("BUSY");
}

static void render_all(void)
{
    lcd_clear();

    draw_center_text(g_ui_status);
    draw_status();
    draw_buttons();
}

static void state_apply(const char *msg)
{
    strncpy(g_message, msg, sizeof(g_message));
    g_message[sizeof(g_message)-1] = '\0';

    ui_update(g_message);

    if (!g_config.is_slave) {
        presence_send_update_all();
    }
}

static void handle_touch(int x, int y)
{
    int w = lcd.width();
    int h = lcd.height();

    int btn_w = w / 3;
    int y_top = 0;

    if (y < y_top)
        return;

    if (x < btn_w) {
        state_apply("ONLINE");
    } else if (x < btn_w * 2) {
        state_apply("AWAY");
    } else {
        state_apply("BUSY");
    }
}

void touch_task(void *arg)
{
    while (1) {

        uint16_t x, y;

        if (lcd.getTouch(&x, &y)) {

            ESP_LOGI("touch", "x=%d y=%d", x, y);

            handle_touch(x, y);
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
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

    xTaskCreate(
        touch_task,
        "touch_task",
        4096,
        NULL,
        5,
        NULL
    );

}

/* ===== メッセージ表示 ===== */
void ui_update(const char *msg)
{
    ESP_LOGI(TAG, "ui_update: %s", msg);
    snprintf(g_ui_status, sizeof(g_ui_status), "%s", msg);
    render_all();
}

/* ===== ネットワーク表示 ===== */
void ui_update_network(void)
{
    render_all();  // ★状態変えず再描画だけ
}

const char* ui_get_status(void)
{
    return g_ui_status;
}

