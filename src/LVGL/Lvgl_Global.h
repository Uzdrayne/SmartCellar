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

//Screen Home
void Initialize_ScreenHome();
void Refresh_ScreenHome();

//Screen Temp
void Initialize_ScreenTemp();
void Refresh_ScreenTemp();

//Screen Hum
void Initialize_ScreenHum();
void Refresh_ScreenHum();

//Screen Fan
void Initialize_ScreenFan();
void Refresh_ScreenFan();

//Screen Wifi
void Initialize_ScreenWifi();
void Refresh_ScreenWifi();

//Screen Warning
void Initialize_ScreenWarning();
void Refresh_ScreenWarning();

#endif