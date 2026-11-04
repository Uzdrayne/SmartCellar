#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "LVGL/Lvgl_Global.h"

extern "C" void ui_event_ScreenHum(lv_event_t * e);

namespace {

lv_obj_t * sensorFaultBanner = nullptr;
bool measurementsAvailable = false;

void ApplyHumidityLock() {
    /*   const bool editingEnabled = measurementsAvailable &&
      //  lv_obj_has_state(ui_SwitchLockHum, LV_STATE_CHECKED);
    if (editingEnabled) {
      //  lv_obj_clear_state(ui_ArcSetHum, LV_STATE_DISABLED);
    } else {
     //   lv_obj_add_state(ui_ArcSetHum, LV_STATE_DISABLED);
    }
    */
 
}

void OnHumidityLockChanged(lv_event_t * e) {
    if (!measurementsAvailable) {
     //   lv_obj_clear_state(ui_SwitchLockHum, LV_STATE_CHECKED);
    }
    ApplyHumidityLock();
}

void OnHumidityScreenLoaded(lv_event_t * e) {
   // lv_obj_clear_state(ui_SwitchLockHum, LV_STATE_CHECKED);
    Refresh_ScreenHum();
}

void OnHumidityGesture(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
   // if (indev == nullptr || lv_obj_has_state(ui_SwitchLockHum, LV_STATE_CHECKED)) {
   //     return;
   // }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_BOTTOM) {
        lv_indev_wait_release(indev);
        _ui_screen_change(&ui_ScreenTemp, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenTemp_screen_init);
    } else if (direction == LV_DIR_TOP) {
        lv_indev_wait_release(indev);
        _ui_screen_change(&ui_ScreenFan, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenFan_screen_init);
    }
}

}

void Initialize_ScreenHum() {
  //  if (ui_ScreenHum == nullptr || ui_SwitchLockHum == nullptr || ui_ArcSetHum == nullptr) {
  //      return;
 //   }

 /*    sensorFaultBanner = CreateSensorFaultBanner(ui_ScreenHum);
    lv_obj_remove_event_cb(ui_ScreenHum, ui_event_ScreenHum);
    lv_obj_add_event_cb(ui_ScreenHum, OnHumidityGesture, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(ui_ScreenHum, OnHumidityScreenLoaded, LV_EVENT_SCREEN_LOADED, nullptr);
    lv_obj_add_event_cb(ui_SwitchLockHum, OnHumidityLockChanged, LV_EVENT_VALUE_CHANGED, nullptr);
*/

   // lv_obj_clear_state(ui_SwitchLockHum, LV_STATE_CHECKED);
    Refresh_ScreenHum();
}

void Refresh_ScreenHum() {
   // if (ui_ScreenHum == nullptr || ui_SwitchLockHum == nullptr ||
   //     ui_ArcSetHum == nullptr || ui_LabelCurrentHum == nullptr) {
   //     return;
   // }

    const StrctDataController data = GetControllerSnapshot();
    measurementsAvailable = HasValidControllerMeasurements(data);
    SetSensorFaultBanner(sensorFaultBanner, measurementsAvailable);

    if (measurementsAvailable) {
        const float humidity = data.Humidity +
            (data.HumOffsetEnabled ? data.HumOffset : 0.0f);
        char text[16];
        snprintf(text, sizeof(text), "%04.1f", humidity);
       // lv_label_set_text(ui_LabelCurrentHum, text);
    } else {
      //  lv_label_set_text(ui_LabelCurrentHum, "--.-");
       // lv_obj_clear_state(ui_SwitchLockHum, LV_STATE_CHECKED);
    }

    ApplyHumidityLock();
}
