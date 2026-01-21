#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Wifi/my_wifi.h"


bool StateSwitchAP = false;

void ScreenWifi_Loaded(lv_event_t * e){

    StateSwitchAP = false;



    set_switch_state(ui_SwitchApEnabled,StateSwitchAP);

    lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(200, 0, 0),0);
    lv_label_set_text(ui_LabelWifiApEnabled,"OFF");

    lv_label_set_text(ui_LabelAPSsid,APSsid.c_str());
    lv_label_set_text(ui_LabelAPPass,APPass.c_str());

    

}



void SwitchAPEnable(lv_event_t * e){

    if(lv_obj_has_state(ui_SwitchApEnabled, LV_STATE_CHECKED)) {
        // Le switch est sur ON
        Serial.println("démarrage du mode AP");

            
        WiFi.disconnect();
        delay(1000);
        WiFi.softAP("MonESP32", "motdepasse123");

        // Récupération et affichage de l'IP
        IPAddress ip = WiFi.softAPIP();
        Serial.print("IP du point d'accès : ");
        Serial.println(ip);


        lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(0, 200, 0),0);
        lv_label_set_text(ui_LabelWifiApEnabled,"ON");

        delay(500);

        
    

    } else {
        // Le switch est sur OFF
        Serial.println("Switch est DÉCOCHÉ");



    }


}