#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"



void ScreenGlobal_Loaded(lv_event_t * e){

    if (Timer_ScreenRefresh == nullptr) {
        Timer_ScreenRefresh = lv_timer_create(CallBackTimer_ScreenRefresh, 500, NULL);
        lv_timer_set_repeat_count(Timer_ScreenRefresh, -1); // Répéter à l'infini
        lv_timer_ready(Timer_ScreenRefresh); // Prêt immédiatement
    }

}


void Refresh_ScreenGlobal(){

    //wifi
    if(WiFi.status() == WL_CONNECTED){
        lv_obj_set_style_text_color(ui_LabelGlobWifi, lv_color_make(0, 200, 0),0);
        lv_label_set_text(ui_LabelGlobWifi, "Wifi OK");
    }
    else{
        lv_obj_set_style_text_color(ui_LabelGlobWifi, lv_color_make(200, 0, 0),0);
        lv_label_set_text(ui_LabelGlobWifi, "NO Wifi");
    }

    
    //link
    if(LinkStatus) {
        lv_obj_set_style_text_color(ui_LabelGlobLink, lv_color_make(0, 200, 0),0);
        sprintf(bufftext, "link ok %s", LinkHeart ? "-" : " ");
        lv_label_set_text(ui_LabelGlobLink, bufftext);
        lv_obj_set_style_arc_color(ui_SpinnerGlob, lv_color_hex(0x36F1FA), LV_PART_INDICATOR | LV_STATE_DEFAULT);

        //Date time
        sprintf(bufftext,"time %04d",millis());
        lv_label_set_text(ui_LabelGlobDate, bufftext);

        //Temp
        sprintf(bufftext,"%05.2f",DataController.TemperatureCorr);
        lv_label_set_text(ui_LabelGlobCurrentTemp, bufftext);
        sprintf(bufftext,"%05.2f°C",DataController.SetTemp);
        lv_label_set_text(ui_LabelGlobSetTemp, bufftext);

        //Hum
        sprintf(bufftext,"%04.1f",DataController.HumidityCorr);
        lv_label_set_text(ui_LabelGlobCurrentHum, bufftext);
        sprintf(bufftext,"%04.1f°C",DataController.SetHum);
        lv_label_set_text(ui_LabelGlobSetHum, bufftext); 

        //Relais
        set_switch_state(ui_SwitchGlobColdState,DataController.ColdState);
        set_switch_state(ui_SwitchGlobHeatState,DataController.HeatState);
        set_switch_state(ui_SwitchGlobDehumState,DataController.DehumState);
        set_switch_state(ui_SwitchGlobHumState,DataController.HumState);

        set_switch_state(ui_SwitchGlobFanState,false);


    } else {
        lv_obj_set_style_text_color(ui_LabelGlobLink, lv_color_make(200, 0, 0),0);
        lv_label_set_text(ui_LabelGlobLink, "NO link");
        lv_obj_set_style_arc_color(ui_SpinnerGlob, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);

        //Date time
        sprintf(bufftext,"time %04d",millis());
        lv_label_set_text(ui_LabelGlobDate, bufftext);

        //Temp
        lv_label_set_text(ui_LabelGlobCurrentTemp, "--.--");
        lv_label_set_text(ui_LabelGlobSetTemp, "--.--");

        //Hum
        lv_label_set_text(ui_LabelGlobCurrentHum, "--.-");
        lv_label_set_text(ui_LabelGlobSetHum, "--.-"); 

        //Relais
        set_switch_state(ui_SwitchGlobColdState,false);
        set_switch_state(ui_SwitchGlobHeatState,false);
        set_switch_state(ui_SwitchGlobDehumState,false);
        set_switch_state(ui_SwitchGlobHumState,false);

        set_switch_state(ui_SwitchGlobFanState,false);
    }




}

