#include "Global/Global.h"

#include <Arduino.h>
#include <lvgl.h>
#include <stdint.h>

#include "DisplayAndTouch/ConfigDisplay.h"

unsigned long LastTouch = 0;
u_int8_t ScreenUserBrightness = 255;
bool ScreenAutoOff = true;
unsigned long ScreenAutoOffTime = 60000;
bool ScreenON = true;
bool UiReady = false;
lv_timer_t *Timer_ScreenRefresh = nullptr;

bool TryingWifi = false;
unsigned long WifiAttemptStart = 0;
bool WifiSTA_OK = false;
bool dnsActive = false;

char bufftext[100];

void Handle_Screen_Brightness(const unsigned long now) {
    const int32_t touchAgeMs = static_cast<int32_t>(now - LastTouch);

    if (ScreenAutoOff && ScreenON &&
        touchAgeMs >= static_cast<int32_t>(ScreenAutoOffTime)) {
        ScreenON = false;
        tft.setBrightness(10);
        Serial.println("Mode Sommeil : Luminosité réduite");
    } else if (!ScreenON && touchAgeMs >= 0 && touchAgeMs < 500) {
        ScreenON = true;
        tft.setBrightness(ScreenUserBrightness);
        Serial.println("Mode Actif : Luminosité rétablie");
    }
}
