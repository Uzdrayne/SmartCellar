#include "DisplayAndTouch/ConfigDisplay.h"

#include <Arduino.h>
#include <lvgl.h>
#include <CST816D.h>

#include "DisplayAndTouch/lgfx_conf.hpp"
#include "Global/Global.h"


LGFX tft;
CST816D touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);


static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;
static const uint32_t Nbline = 10 ;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][screenWidth * Nbline];

#if LV_USE_LOG 
/* Serial debugging */
void my_print(const char *buf)
{
  Serial.printf("%s\r\n", buf);
  Serial.flush();
}
#endif



void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  if (tft.getStartCount() == 0)
  {
    tft.endWrite();
  }

  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::swap565_t *)&color_p->full);

  lv_disp_flush_ready(disp); 
}



/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{

  unsigned long now =millis();

  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY, &gesture);

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    LastTouch=now;

    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}


void my_TouchScreen_Init(){



  tft.init();
  tft.initDMA();
  //tft.startWrite();
 
  touch.begin();
  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], screenWidth * Nbline);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  indev_drv.long_press_time = 1000;

  lv_indev_drv_register(&indev_drv);

}


