#ifndef MY_GLOBAL_H
#define MY_GLOBAL_H

#include <Arduino.h>
#include <lvgl.h>

#include "Data/ControllerData.h"

extern bool TryingWifi;
extern unsigned long WifiAttemptStart;
extern bool WifiSTA_OK;
extern bool dnsActive;

extern unsigned long LastTouch;
extern u_int8_t ScreenUserBrightness;
extern bool ScreenAutoOff;
extern unsigned long ScreenAutoOffTime;
extern bool ScreenON;
extern bool UiReady;

extern lv_timer_t *Timer_ScreenRefresh;
extern char bufftext[];

void Handle_Screen_Brightness(unsigned long now);

#endif
