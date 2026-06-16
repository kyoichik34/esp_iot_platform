#include "ui.h"
#include "LGFX_ESP32S3_WT32_SC01_Plus.hpp"
#include <LovyanGFX.hpp>

static LGFX lcd;

void ui_init(void)
{
    lcd.init();
    lcd.fillScreen(TFT_BLACK);
}

void ui_update(const char* msg)
{
    lcd.setCursor(0, 0);
    lcd.setTextColor(TFT_WHITE);
    lcd.fillScreen(TFT_BLACK);

    lcd.println("ESP32 Presence");
    lcd.println("----------------");
    lcd.println(msg);
}
