#include "LVGL/Lvgl_Global.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <ui.h>
#include "Global/Global.h"



void set_switch_state(lv_obj_t* sw, bool on) {
    if(on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else   lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

StrctDataController GetControllerSnapshot() {
    StrctDataController data;
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    data = DataController;
    xSemaphoreGive(DataMutex);
    return data;
}

bool HasValidControllerMeasurements(const StrctDataController& data) {
    const float temperature = data.Temperature +
        (data.TempOffsetEnabled ? data.TempOffset : 0.0f);
    const float humidity = data.Humidity +
        (data.HumOffsetEnabled ? data.HumOffset : 0.0f);

    return isfinite(data.Temperature) && isfinite(data.Humidity) &&
        isfinite(temperature) && temperature >= -40.0f && temperature <= 80.0f &&
        isfinite(humidity) && humidity >= 0.0f && humidity <= 100.0f;
}

lv_obj_t* CreateSensorFaultBanner(lv_obj_t* parent) {
    lv_obj_t* banner = lv_label_create(parent);
    if (banner == nullptr) {
        Serial.println("[LVGL] Impossible de creer le bandeau d'alerte capteur");
        return nullptr;
    }

    lv_label_set_text(banner, "MESURES INDISPONIBLES");
    lv_obj_set_align(banner, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(banner, 6, 6);
    lv_obj_set_style_text_color(banner, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(banner, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0xB00020), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(banner, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(banner, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
    return banner;
}

void SetSensorFaultBanner(lv_obj_t* banner, bool measurementsAvailable) {
    if (banner == nullptr) {
        return;
    }

    if (measurementsAvailable) {
        lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(banner, LV_OBJ_FLAG_HIDDEN);
    }
}

void Initialize_LvglUI() {
    ui_init();


    //créer le time de 500ms pour rafraichir les données à l'écran
    if (Timer_ScreenRefresh == nullptr) {
        Timer_ScreenRefresh = lv_timer_create(CallBackTimer_ScreenRefresh, 500, NULL);
        lv_timer_set_repeat_count(Timer_ScreenRefresh, -1); // Répéter à l'infini
        lv_timer_ready(Timer_ScreenRefresh); // Prêt immédiatement
    }

    Initialize_ScreenHome();
    Initialize_ScreenTemp();

    Initialize_ScreenHum();
    Initialize_ScreenFan();
    Initialize_ScreenWifi();
    Initialize_ScreenWarning();
}

//timer pour refresh le temps du chrono
void CallBackTimer_ScreenRefresh(lv_timer_t * Timer_ScreenRefresh){

    
    if(UiReady){
        Handle_ScreenWarningNavigation(millis());

        if (lv_scr_act() == ui_ScreenHome) {
            Refresh_ScreenHome();
        } 
        else if (lv_scr_act() == ui_ScreenTemp) {
            Refresh_ScreenTemp();
        } 
        else if (lv_scr_act() == ui_ScreenHum) {
            Refresh_ScreenHum();
        }
        else if (lv_scr_act() == ui_ScreenFan) {
            Refresh_ScreenFan();
        }
        else if (lv_scr_act() == ui_ScreenWifi) {
            Refresh_ScreenWifi();
        }        
        else if (lv_scr_act() == ui_ScreenWarning) {
            Refresh_ScreenWarning();
        }


    }
    
}