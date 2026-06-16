#pragma once

#define LGFX_USE_V1
#define WT32_SC01_PLUS


#define	EXT_IO1	GPIO10
#define	EXT_IO2	GPIO11
#define	EXT_IO3	GPIO12
#define	EXT_IO4	GPIO13
#define	EXT_IO5	GPIO14
#define	EXT_IO6	GPIO21

#define	ESP_RXD	GPIO44
#define	ESP_TXD	GPIO43

#define	RXD	1
#define	RTS	2


#include <LovyanGFX.hpp>

// LGFX for WT32-SC01 PLUS
// https://gist.github.com/sukesh-ak/610508bc84779a26efdcf969bf51a2d1

class LGFX : public lgfx::LGFX_Device
{

  lgfx::Panel_ST7796  _panel_instance;  // ST7796UI
  lgfx::Bus_Parallel8 _bus_instance;    // MCU8080 8B
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_FT5x06  _touch_instance;  // FT6336U

public:

  LGFX(void)
  {
    { // バス制御の設定を行います。
      auto cfg = _bus_instance.config();      // バス設定用の構造体を取得します。
      cfg.freq_write = 20000000;
      cfg.pin_wr = 47;
      cfg.pin_rd = -1;
      cfg.pin_rs = 0;

      // LCD data interface, 8bit MCU (8080)
      cfg.pin_d0 = 9;
      cfg.pin_d1 = 46;
      cfg.pin_d2 = 3;
      cfg.pin_d3 = 8;
      cfg.pin_d4 = 18;
      cfg.pin_d5 = 17;
      cfg.pin_d6 = 16;
      cfg.pin_d7 = 15;

      _bus_instance.config(cfg);  // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
    }

    { // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config();    // 表示パネル設定用の構造体を取得します。

      cfg.pin_cs           = -1;
      cfg.pin_rst          = 4;
      cfg.pin_busy         = -1;

      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;

      _panel_instance.config(cfg);
    }

    { // バックライト制御の設定を行います。
      auto cfg = _light_instance.config();     // バックライト設定用の構造体を取得します。

      cfg.pin_bl      = 45;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance); // バックライトをパネルにセットします。
    }

    { // タッチスクリーン制御の設定を行います。
      auto cfg = _touch_instance.config();

      cfg.x_min           = 0;
      cfg.x_max           = 319;
      cfg.y_min           = 0;
      cfg.y_max           = 479;
      cfg.pin_int         = 7;
      cfg.bus_shared      = true;
      cfg.offset_rotation = 0;

      cfg.i2c_port        = 1;
      cfg.i2c_addr        = 0x38;
      cfg.pin_sda         = 6;
      cfg.pin_scl         = 5;
      cfg.freq            = 400000;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance); // タッチスクリーンをパネルにセットします。
    }
    setPanel(&_panel_instance); // 使用するパネルをセットします。
  }
};
