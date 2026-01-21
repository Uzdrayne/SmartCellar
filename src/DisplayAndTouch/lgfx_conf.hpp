#pragma once

#define LGFX_USE_V1
#define LGFX_NO_FONT
#include <LovyanGFX.hpp>


#define I2C_SDA 4
#define I2C_SCL 5
#define TP_INT 0
#define TP_RST 1

//#define off_pin 35


class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light;
public:
  LGFX()
  {
    // -------- SPI BUS CONFIG --------
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host = SPI2_HOST;     // ESP32-C3
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 20000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 2;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // -------- PANEL CONFIG --------
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs   = 10;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;

      cfg.memory_width  = 240;
      cfg.memory_height = 240;
      cfg.panel_width   = 240;
      cfg.panel_height  = 240;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;

      cfg.readable  = false;
      cfg.invert    = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light.config();

      cfg.pin_bl      = 3;     // Backlight pin
      cfg.invert      = false;
      cfg.freq        = 5000;  // PWM frequency
      cfg.pwm_channel = 0;

      _light.config(cfg);
      _panel_instance.setLight(&_light);
    }


    setPanel(&_panel_instance);
  }
};

