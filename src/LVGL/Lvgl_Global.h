#ifndef LVGL_GLOBAL_H
#define LVGL_GLOBAL_H

#include <Arduino.h>
#include <lvgl.h>

#include "Global/Global.h"



void set_switch_state(lv_obj_t* sw, bool on);
StrctDataController GetControllerSnapshot();
bool HasValidControllerMeasurements(const StrctDataController& data);
lv_obj_t* CreateSensorFaultBanner(lv_obj_t* parent);
void SetSensorFaultBanner(lv_obj_t* banner, bool measurementsAvailable);


void CallBackTimer_ScreenRefresh(lv_timer_t * Timer_ScreenRefresh);

void Initialize_LvglUI();
void Refresh_ScreenGlobal();
void Refresh_ScreenTemp();
void Refresh_ScreenHum();
void Refresh_ScreenFan();
void Refresh_ScreenWifi();
void Initialize_ScreenTemp();
void Initialize_ScreenHum();
void Initialize_ScreenFan();


#endif