#ifndef LVGL_GLOBAL_H
#define LVGL_GLOBAL_H

#include <Arduino.h>
#include <lvgl.h>

#include "Global/Global.h"



void set_switch_state(lv_obj_t* sw, bool on);


void CallBackTimer_ScreenRefresh(lv_timer_t * Timer_ScreenRefresh);

void Refresh_ScreenGlobal();
void Refresh_ScreenWifi();


#endif