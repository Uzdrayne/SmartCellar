#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

extern "C" void ui_event_ScreenTemp(lv_event_t * e);

namespace {

lv_obj_t * sensorFaultBanner = nullptr;
bool measurementsAvailable = false;

void ApplyTemperatureLock() {
    /*
    const bool editingEnabled = measurementsAvailable &&
        lv_obj_has_state(ui_SwitchLockTemp, LV_STATE_CHECKED);
    if (editingEnabled) {
        lv_obj_clear_state(ui_ArcSetTemp, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_ArcSetTemp, LV_STATE_DISABLED);
    }
    */
}

void OnTemperatureLockChanged(lv_event_t * e) {
    if (!measurementsAvailable) {
      //  lv_obj_clear_state(ui_SwitchLockTemp, LV_STATE_CHECKED);
    }
    ApplyTemperatureLock();
}

void OnTemperatureScreenLoaded(lv_event_t * e) {
   // lv_obj_clear_state(ui_SwitchLockTemp, LV_STATE_CHECKED);
    Refresh_ScreenTemp();
}

void OnTemperatureGesture(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
   // if (indev == nullptr || lv_obj_has_state(ui_SwitchLockTemp, LV_STATE_CHECKED)) {
   //     return;
   // }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_TOP) {
        lv_indev_wait_release(indev);
        _ui_screen_change(&ui_ScreenHum, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenHum_screen_init);
    } else if (direction == LV_DIR_BOTTOM) {
        lv_indev_wait_release(indev);
       // _ui_screen_change(&ui_ScreenGlobal, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGlobal_screen_init);
    }
}

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
    Refresh_ScreenTemp();
    */
}

void Refresh_ScreenTemp() {
    /*
    if (ui_ScreenTemp == nullptr || ui_SwitchLockTemp == nullptr ||
        ui_ArcSetTemp == nullptr || ui_LabelCurrentTemp == nullptr) {
        return;
    }

    const StrctDataController data = GetControllerSnapshot();
    measurementsAvailable = HasValidControllerMeasurements(data);
    SetSensorFaultBanner(sensorFaultBanner, measurementsAvailable);

    if (measurementsAvailable) {
        const float temperature = data.Temperature +
            (data.TempOffsetEnabled ? data.TempOffset : 0.0f);
        char text[16];
        snprintf(text, sizeof(text), "%05.2f", temperature);
        lv_label_set_text(ui_LabelCurrentTemp, text);
    } else {
        lv_label_set_text(ui_LabelCurrentTemp, "--.--");
        lv_obj_clear_state(ui_SwitchLockTemp, LV_STATE_CHECKED);
    }

    ApplyTemperatureLock();

    */
}
