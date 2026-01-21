#ifndef CONFIG_DISPLAY_H
#define  CONFIG_DISPLAY_H

#include <Arduino.h>
#include <CST816D.h>
#include <lvgl.h>
#include "DisplayAndTouch/lgfx_conf.hpp"

extern LGFX tft;
extern CST816D touch;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);


void my_TouchScreen_Init();



#endif