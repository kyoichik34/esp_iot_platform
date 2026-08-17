#include "presence.h"
#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include "wifi.h"
#include "config.h"
#include "http_server.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "ui";

static LGFX lcd;

/* ===== 外部状態 ===== */
extern char g_ipv4_str[32];
extern char g_ipv6_str[64];
extern int  g_ipv4_ready;
extern int  g_ipv6_ready;

/* ===== UI状態 ===== */
const char *ui_status_str[] = {
    "待機中",
    "在席中",
    "退席中",
    "移動中"
};

/* ===== UIイベント ===== */
static QueueHandle_t ui_queue;
static presence_state_t state_render = {
    .status = STATUS_READY,
    .comment = ""
};

/* ===== 描画 ===== */
#define BUTTON_Y      0
#define BUTTON_H      40

static void lcd_clear(void)
{
    lcd.fillScreen(TFT_BLACK);
}

static void draw_status(void)
{
    int width  = lcd.width();
    int height = lcd.height();

    char line1[128];
    snprintf(line1, sizeof(line1),
             "%s",
             g_config.hostname);

    lcd.setTextSize(1.8);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);

    /*
     * 1行目
     * hostname 左
     * IPv4 右
     */
    lcd.setCursor(0, height - 40);
    lcd.print(line1);
    snprintf(line1, sizeof(line1),
             "%s",
             g_ipv4_ready ? g_ipv4_str : "[IPv4 is not available]");

    int ipv4_w = lcd.textWidth(line1);
    lcd.setCursor(width - ipv4_w,
                  height - 40);
    lcd.print(line1);

    /*
     * 2行目
     * IPv6 全幅
     */
    snprintf(line1, sizeof(line1),
             "%s",
             g_ipv6_ready ? g_ipv6_str : "IPv6 is not available");
    int ipv6_w = lcd.textWidth(line1);

    lcd.setCursor(width - ipv6_w,
                  height - 20);
    lcd.print(line1);
}

static void draw_center_text(const char *text)
{
    if (text == NULL)
        return;

    int screen_w = lcd.width();
    int screen_h = lcd.height();
    const int text_h = 72;

    lcd.setFont(&fonts::lgfxJapanGothic_24);
    lcd.setTextSize(3);
    lcd.setTextColor(TFT_WHITE, TFT_BLUE);

    int text_w = lcd.textWidth(text);
    int x = (screen_w - text_w) / 2;
    int y = (screen_h - text_h) / 2;
    y = BUTTON_H + 24;

    // ★文字領域全体をクリア
    lcd.fillRect(
        0,
        y - 6,
        screen_w,
        text_h + 12,
        TFT_CYAN
    );

    lcd.setCursor(x, y);
    lcd.print(text);

    lcd.setFont(nullptr);
}

static void draw_comment(const char *comment)
{
    if (comment == NULL)
        return;

    int screen_w = lcd.width();
    int screen_h = lcd.height();

    lcd.setFont(&fonts::lgfxJapanGothic_24);
    lcd.setTextSize(2.5);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);

    /* 状態表示の少し下 */
    int y = (screen_h / 2);

    /* コメント領域だけ消す */
    lcd.fillRect(
        0,
        y,
        screen_w,
        screen_h - y - 40,
        TFT_BLACK
    );

    int text_w = lcd.textWidth(comment);
    int x = (screen_w - text_w) / 2;

    if (x < 0)
        x = 0;

    lcd.setCursor(x, y);
    lcd.print(comment);

    lcd.setFont(nullptr);
}

static void draw_buttons(void)
{
    int w = lcd.width();
    int btn_w = w / 3;
    int y = BUTTON_Y;

    // lcd.setTextSize(2);
    lcd.setFont(&fonts::efontJA_24);
    lcd.setTextSize(1.2);

    lcd.fillRect(0, y, btn_w, 40, TFT_GREEN);
    lcd.setCursor(10, y + 8);
    lcd.setTextColor(TFT_BLACK);
    lcd.print(ui_status_str[STATUS_ONLINE]);

    lcd.fillRect(btn_w, y, btn_w, 40, TFT_YELLOW);
    lcd.setCursor(btn_w + 10, y + 8);
    lcd.print(ui_status_str[STATUS_AWAY]);

    lcd.fillRect(btn_w * 2, y, btn_w, 40, TFT_RED);
    lcd.setCursor(btn_w * 2 + 10, y + 8);
    lcd.print(ui_status_str[STATUS_BUSY]);

    lcd.setFont(nullptr);
}

static void render_all(void)
{
    lcd_clear();
    if( state_render.status < STATUS_MAX)
        draw_center_text(ui_status_str[state_render.status]);
    draw_comment(state_render.comment);
    draw_status();
    draw_buttons();
}

/* ===== UIタスク ===== */
static void ui_task(void *arg)
{
    while (1) {
        if (xQueueReceive(ui_queue, &state_render, portMAX_DELAY)) {
            ESP_LOGI(TAG, "ui_status: %d, comment: %s", state_render.status, state_render.comment);
            render_all();
        }
    }
}

/* ===== API ===== */

void ui_update(presence_state_t *status)
{
    xQueueSend(ui_queue, status, 0);
}

/* ===== 状態適用 ===== */

static void state_apply(status_t status)
{
    g_presence.status = status;
    ui_update(&g_presence);

    if (g_config.send_update) {
        presence_send_update_all_with();
        ESP_LOGI(TAG, "send_update : %d", g_presence.status);
    }
}	

/* ===== タッチ ===== */
static void handle_touch(int x, int y)
{
    int w = lcd.width();
    int btn_w = w / 3;

    if (y < BUTTON_Y || y >= (BUTTON_Y + BUTTON_H)) {
        return;
    }

    status_t status;

    if (x < btn_w) {
        status = STATUS_ONLINE;
    } else if (x < btn_w * 2) {
        status = STATUS_AWAY;
    } else {
        status = STATUS_BUSY;
    }

    state_apply(status);
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
    lcd.setRotation(1);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    ui_queue = xQueueCreate(4, sizeof(presence_state_t));
    ui_update(&g_presence);
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
}

