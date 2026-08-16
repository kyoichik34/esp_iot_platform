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
    const int text_h = 50;

    lcd.setTextSize(4.5);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    int text_w = lcd.textWidth(text);
    int x = (screen_w - text_w) / 2;
    int y = (screen_h - text_h) / 2;

    // ★文字領域全体をクリア
    lcd.fillRect(
        0,
        y - 10,
        screen_w,
        text_h + 20,
        TFT_BLACK
    );

    lcd.setCursor(x, y);
    lcd.print(text);
}

static void draw_comment(const char *comment)
{
    if (comment == NULL)
        return;

    int screen_w = lcd.width();
    int screen_h = lcd.height();

    lcd.setFont(&fonts::lgfxJapanGothic_24);
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);

    /* 状態表示の少し下 */
    int y = (screen_h / 2) + 10;

    /* コメント領域だけ消す */
    lcd.fillRect(
        0,
        y - 4,
        screen_w,
        60,
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
    draw_center_text(g_presence.status);
    draw_comment(g_presence.comment);
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
    strncpy(g_presence.status, msg, sizeof(g_presence.status));
    g_presence.status[sizeof(g_presence.status) - 1] = '\0';

    ui_update(g_presence.status);

    if (g_config.send_update) {
        presence_send_update_all_with();
        ESP_LOGI(TAG, "send_update : %s", msg);
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

    status_t g_status;

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
