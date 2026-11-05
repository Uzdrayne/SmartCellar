#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"




/*
ui_TempArcSetTemp : Arc pour régler la température souhaitée

ui_TempLabelSetTemp : Label affichant la consigne detempérature (XX.X sans unité)
ui_TempLabelCurrentTemp : Label affichant la température actuelle (XX.X sans unité)

ui_TempSwitchHeatState : Switch indiquant l'état du chauffage (ON/OFF)
ui_TempSwitchColdState : Switch indiquant l'état de la climatisation (ON/OFF)

ui_TempSwitchOption : Switch peour aller au screen option temperature

ui_TempSwitchLock : Switch pour verrouiller le screen temperature
*/

namespace {

    bool measurementsAvailable = false;

}

void Initialize_ScreenTemp() {
    /*
    if (ui_ScreenTemp == nullptr || ui_SwitchLockTemp == nullptr || ui_ArcSetTemp == nullptr) {
        return;
    }

    sensorFaultBanner = CreateSensorFaultBanner(ui_ScreenTemp);
    lv_obj_remove_event_cb(ui_ScreenTemp, ui_event_ScreenTemp);
    lv_obj_add_event_cb(ui_ScreenTemp, OnTemperatureGesture, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(ui_ScreenTemp, OnTemperatureScreenLoaded, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(ui_SwitchLockTemp, OnTemperatureLockChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_clear_state(ui_SwitchLockTemp, LV_STATE_CHECKED);
    
    */
   
    Refresh_ScreenTemp();
}

void Refresh_ScreenTemp() {
    /*
    if (ui_ScreenTemp == nullptr || ui_SwitchLockTemp == nullptr ||
        ui_ArcSetTemp == nullptr || ui_LabelCurrentTemp == nullptr) {
        return;
    }
    */

    const StrctDataController data = GetControllerSnapshot();
    measurementsAvailable = HasValidControllerMeasurements(data);

   // SetSensorFaultBanner(sensorFaultBanner, measurementsAvailable);

    if (measurementsAvailable) {
        const float temperature = data.Temperature + (data.TempOffsetEnabled ? data.TempOffset : 0.0f);
        char text[16];
        snprintf(text, sizeof(text), "%04.1f", temperature);
        lv_label_set_text(ui_TempLabelCurrentTemp, text);
    } else {
        lv_label_set_text(ui_TempLabelCurrentTemp, "--.-");
    }


}
