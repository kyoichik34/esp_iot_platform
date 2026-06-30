#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include "wifi.h"
#include "config.h"
#include "presence.h"
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
static char g_ui_status[32] = "READY";

const char *ui_status_str[] = {
    "ONLINE",
    "AWAY",
    "BUSY"
};

/* ===== UIイベント ===== */
typedef struct {
    char msg[32];
} ui_event_t;

static QueueHandle_t ui_queue;

/* ===== 描画 ===== */

static void lcd_clear(void)
{
    lcd.fillScreen(TFT_BLACK);
}

static void draw_status(void)
{
    int width  = lcd.width();
    int height = lcd.height();

    char line1[96];
    char line2[96];
    char line3[96];

    snprintf(line1, sizeof(line1), "%s", g_config.hostname);
    snprintf(line2, sizeof(line2), "v4:%s", g_ipv4_ready ? g_ipv4_str : "-");
    snprintf(line3, sizeof(line3), "v6:%s", g_ipv6_ready ? g_ipv6_str : "-");

    lcd.setTextSize(1.8);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);

    lcd.setCursor(width - lcd.textWidth(line1), height - 80);
    lcd.print(line1);

    lcd.setCursor(width - lcd.textWidth(line2), height - 60);
    lcd.print(line2);

    lcd.setCursor(width - lcd.textWidth(line3), height - 40);
    lcd.print(line3);
}

static void draw_center_text(const char *text)
{
    int screen_w = lcd.width();
    int screen_h = lcd.height();

    lcd.setTextSize(3);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    int text_w = lcd.textWidth(text);
    int x = (screen_w - text_w) / 2;
    int y = (screen_h - 48) / 2;

    lcd.setCursor(x, y);
    lcd.print(text);
}

static void draw_buttons(void)
{
    int w = lcd.width();
    int btn_w = w / 3;
    int y = 0;

    lcd.setTextSize(2);

    lcd.fillRect(0, y, btn_w, 40, TFT_GREEN);
    lcd.setCursor(10, y + 10);
    lcd.setTextColor(TFT_BLACK);
    lcd.print(ui_status_str[STATUS_ONLINE]);

    lcd.fillRect(btn_w, y, btn_w, 40, TFT_YELLOW);
    lcd.setCursor(btn_w + 10, y + 10);
    lcd.print(ui_status_str[STATUS_AWAY]);

    lcd.fillRect(btn_w * 2, y, btn_w, 40, TFT_RED);
    lcd.setCursor(btn_w * 2 + 10, y + 10);
    lcd.print(ui_status_str[STATUS_BUSY]);
}

static void render_all(void)
{
    lcd_clear();
    draw_center_text(g_ui_status);
    draw_status();
    draw_buttons();
}

/* ===== UIタスク ===== */

static void ui_task(void *arg)
{
    ui_event_t ev;

    while (1) {
        if (xQueueReceive(ui_queue, &ev, portMAX_DELAY)) {

            ESP_LOGI(TAG, "ui_event: %s", ev.msg);

            snprintf(g_ui_status, sizeof(g_ui_status), "%s", ev.msg);

            render_all();
        }
    }
}

/* ===== API ===== */

void ui_update(const char *msg)
{
    ui_event_t ev;

    strncpy(ev.msg, msg, sizeof(ev.msg));
    ev.msg[sizeof(ev.msg)-1] = '\0';

    xQueueSend(ui_queue, &ev, 0);
}

/* ===== 状態適用 ===== */

static void state_apply(const char *msg)
{
    strncpy(g_message, msg, sizeof(g_message));
    g_message[sizeof(g_message)-1] = '\0';

    ui_update(g_message);

    if (g_config.send_update) {
        presence_send_update_all_with(msg);
        ESP_LOGI(TAG, "send_update : %s", msg);
    }
}

/* ===== タッチ ===== */

static void handle_touch(int x, int y)
{
    int w = lcd.width();
    int btn_w = w / 3;

    status_t g_status = STATUS_ONLINE;
    if (x < btn_w) {
        g_status = STATUS_ONLINE;
    } else if (x < btn_w * 2) {
        g_status = STATUS_AWAY;
    } else {
        g_status = STATUS_BUSY;
    }
    state_apply(ui_status_str[g_status]);
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

    ui_queue = xQueueCreate(8, sizeof(ui_event_t));

    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);

    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);

    ui_update("BOOT");
}

/* ===== ネットワーク表示 ===== */
void ui_update_network(void)
{
    render_all();  // ★状態変えず再描画だけ
}

/* ===== getter ===== */

const char* ui_get_status(void)
{
    return g_ui_status;
}
