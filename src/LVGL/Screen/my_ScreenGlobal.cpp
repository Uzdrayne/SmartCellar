#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

namespace {

void refreshDateLabel() {
    constexpr time_t minimumValidEpoch = 1577836800; // 2020-01-01 UTC
    const time_t now = time(nullptr);
    if (now < minimumValidEpoch) {
      //  lv_label_set_text(ui_LabelGlobDate, "Attente NTP...");
        return;
    }

    struct tm localTime;
    char dateText[32];
    if (localtime_r(&now, &localTime) == nullptr ||
        strftime(dateText, sizeof(dateText), "%d/%m/%Y %H:%M", &localTime) == 0) {
     //   lv_label_set_text(ui_LabelGlobDate, "Heure indisponible");
        return;
    }

  //  lv_label_set_text(ui_LabelGlobDate, dateText);
}

}



void ScreenGlobal_Loaded(lv_event_t * e){

    if (Timer_ScreenRefresh == nullptr) {
        Timer_ScreenRefresh = lv_timer_create(CallBackTimer_ScreenRefresh, 500, NULL);
        lv_timer_set_repeat_count(Timer_ScreenRefresh, -1); // Répéter à l'infini
        lv_timer_ready(Timer_ScreenRefresh); // Prêt immédiatement
    }

}


void Refresh_ScreenGlobal(){
    const StrctDataController localData = GetControllerSnapshot();

    //wifi
    if(WiFi.status() == WL_CONNECTED){
     //   lv_obj_set_style_text_color(ui_LabelGlobWifi, lv_color_make(0, 200, 0),0);
     //   lv_label_set_text(ui_LabelGlobWifi, "STA OK");
    } else if ((WiFi.getMode() & WIFI_AP) != 0) {
      //  lv_obj_set_style_text_color(ui_LabelGlobWifi, lv_color_make(220, 160, 0),0);
     //   lv_label_set_text(ui_LabelGlobWifi, "Mode AP");
    } else {
     //   lv_obj_set_style_text_color(ui_LabelGlobWifi, lv_color_make(200, 0, 0),0);
      //  lv_label_set_text(ui_LabelGlobWifi, "Sans reseau");
    }

    
    const bool sensorReady = HasValidControllerMeasurements(localData);
    refreshDateLabel();
  //  set_switch_state(ui_SwitchGlobFanState, localData.FanState);
    if(sensorReady) {
      //  lv_obj_set_style_text_color(ui_LabelGlobLink, lv_color_make(0, 200, 0),0);
        sprintf(bufftext, "Liaison OK");
     //   lv_label_set_text(ui_LabelGlobLink, bufftext);
      //  lv_obj_set_style_arc_color(ui_SpinnerGlob, lv_color_hex(0x36F1FA), LV_PART_INDICATOR | LV_STATE_DEFAULT);

        const float correctedTemperature = localData.Temperature +
            (localData.TempOffsetEnabled ? localData.TempOffset : 0.0f);
        const float correctedHumidity = localData.Humidity +
            (localData.HumOffsetEnabled ? localData.HumOffset : 0.0f);
        sprintf(bufftext,"%05.2f",correctedTemperature);
      //  lv_label_set_text(ui_LabelGlobCurrentTemp, bufftext);
        sprintf(bufftext,"%05.2f°C",localData.SetTemp);
      //  lv_label_set_text(ui_LabelGlobSetTemp, bufftext);

        sprintf(bufftext,"%04.1f",correctedHumidity);
     //   lv_label_set_text(ui_LabelGlobCurrentHum, bufftext);
        sprintf(bufftext,"%04.1f%%",localData.SetHum);
     //   lv_label_set_text(ui_LabelGlobSetHum, bufftext); 

     //   set_switch_state(ui_SwitchGlobColdState,localData.ColdState);
     //   set_switch_state(ui_SwitchGlobHeatState,localData.HeatState);
     //   set_switch_state(ui_SwitchGlobDehumState,localData.DehumState);
     //   set_switch_state(ui_SwitchGlobHumState,localData.HumState);

    } else {
      //  lv_obj_set_style_text_color(ui_LabelGlobLink, lv_color_make(200, 0, 0),0);
      //  lv_label_set_text(ui_LabelGlobLink, "Capteur indisponible");
      //  lv_obj_set_style_arc_color(ui_SpinnerGlob, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);

     //   lv_label_set_text(ui_LabelGlobCurrentTemp, "--.--");
        sprintf(bufftext,"%05.2f°C",localData.SetTemp);

     //   lv_label_set_text(ui_LabelGlobSetTemp, bufftext);
     //   lv_label_set_text(ui_LabelGlobCurrentHum, "--.-");
        sprintf(bufftext,"%04.1f%%",localData.SetHum);
     //   lv_label_set_text(ui_LabelGlobSetHum, bufftext);

     //   set_switch_state(ui_SwitchGlobColdState,localData.ColdState);
     //   set_switch_state(ui_SwitchGlobHeatState,localData.HeatState);
     //   set_switch_state(ui_SwitchGlobDehumState,localData.DehumState);
     //   set_switch_state(ui_SwitchGlobHumState,localData.HumState);

    }




}
