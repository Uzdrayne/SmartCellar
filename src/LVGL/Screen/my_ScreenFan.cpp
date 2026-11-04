#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "LVGL/Lvgl_Global.h"

extern "C" void ui_event_ScreenFan(lv_event_t * e);

namespace {

lv_obj_t * sensorFaultBanner = nullptr;

void OnFanScreenLoaded(lv_event_t * e) {
   // lv_obj_clear_state(ui_SwitchLockFan, LV_STATE_CHECKED);
    Refresh_ScreenFan();
}

void OnFanGesture(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
   // if (indev == nullptr || lv_obj_has_state(ui_SwitchLockFan, LV_STATE_CHECKED)) {
   //     return;
   // }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_BOTTOM) {
        lv_indev_wait_release(indev);
        _ui_screen_change(&ui_ScreenHum, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenHum_screen_init);
    } else if (direction == LV_DIR_TOP) {
        lv_indev_wait_release(indev);
        _ui_screen_change(&ui_ScreenWifi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenWifi_screen_init);
    }
}

}

void Initialize_ScreenFan() {
  //  if (ui_ScreenFan == nullptr || ui_SwitchLockFan == nullptr) {
 //       return;
 //   }

    sensorFaultBanner = CreateSensorFaultBanner(ui_ScreenFan);
    lv_obj_remove_event_cb(ui_ScreenFan, ui_event_ScreenFan);
    lv_obj_add_event_cb(ui_ScreenFan, OnFanGesture, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(ui_ScreenFan, OnFanScreenLoaded, LV_EVENT_SCREEN_LOADED, nullptr);

   // lv_obj_clear_state(ui_SwitchLockFan, LV_STATE_CHECKED);
    Refresh_ScreenFan();
}

void Refresh_ScreenFan() {
  //  if (ui_ScreenFan == nullptr || ui_SwitchLockFan == nullptr) {
  //      return;
 //   }

    const StrctDataController data = GetControllerSnapshot();
    const bool measurementsAvailable = HasValidControllerMeasurements(data);
    SetSensorFaultBanner(sensorFaultBanner, measurementsAvailable);
    if (!measurementsAvailable) {
      //  lv_obj_clear_state(ui_SwitchLockFan, LV_STATE_CHECKED);
    }
}
