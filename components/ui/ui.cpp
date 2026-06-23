#include "ui.h"
#include "wifi.h"

#include <stdio.h>
#include <string.h>

/* ===== 外部状態 ===== */
extern char g_ipv4_str[32];
extern char g_ipv6_str[64];
extern int  g_ipv4_ready;
extern int  g_ipv6_ready;

/* ===== LCD抽象（ここを自分の環境に置き換える） ===== */

static void lcd_clear(void)
{
    // TODO: 実装に合わせて書き換え
}

static void lcd_draw_string(int x, int y, const char *str)
{
    // TODO: 実装に合わせて書き換え
}

/* ===== 初期化 ===== */
void ui_init(void)
{
    lcd_clear();

    lcd_draw_string(0, 0, "ESP32 Presence");
    lcd_draw_string(0, 16, "Starting...");
}

/* ===== メッセージ表示 ===== */
void ui_update(const char *msg)
{
    lcd_clear();

    char buf[64];

    snprintf(buf, sizeof(buf),
             "MSG: %.32s", msg);

    lcd_draw_string(0, 0, buf);
}

/* ===== ネットワーク表示 ===== */
void ui_update_network(void)
{
    lcd_clear();

    char line1[64];
    char line2[64];

    /* IPv4 */
    snprintf(line1, sizeof(line1),
             "IPv4 %c %s",
             g_ipv4_ready ? 'O' : 'X',
             g_ipv4_ready ? g_ipv4_str : "-");

    /* IPv6（長いので省略） */
    snprintf(line2, sizeof(line2),
             "IPv6 %c %.20s",
             g_ipv6_ready ? 'O' : 'X',
             g_ipv6_ready ? g_ipv6_str : "-");

    lcd_draw_string(0, 0, line1);
    lcd_draw_string(0, 16, line2);
}
