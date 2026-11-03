#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <ui.h>
#include <string.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Wifi/my_wifi.h"

namespace {

void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
    if (label == nullptr) {
        return;
    }

    const char* currentText = lv_label_get_text(label);
    if (currentText == nullptr || strcmp(currentText, text) != 0) {
        lv_label_set_text(label, text);
    }
}

void formatIpAddress(char* text, size_t textSize, const IPAddress& ip) {
    snprintf(text, textSize, "%u.%u.%u.%u",
             static_cast<unsigned int>(ip[0]),
             static_cast<unsigned int>(ip[1]),
             static_cast<unsigned int>(ip[2]),
             static_cast<unsigned int>(ip[3]));
}

}

bool StateSwitchAP = false;

void ScreenWifi_Loaded(lv_event_t * e){

    StateSwitchAP = (WiFi.getMode() & WIFI_AP) != 0;

    set_switch_state(ui_SwitchApEnabled,StateSwitchAP);

    const lv_color_t statusColor = StateSwitchAP
        ? lv_color_make(0, 200, 0)
        : lv_color_make(200, 0, 0);
    lv_obj_set_style_text_color(ui_LabelWifiApEnabled, statusColor, 0);
    lv_label_set_text(ui_LabelWifiApEnabled, StateSwitchAP ? "ON" : "OFF");

    lv_label_set_text(ui_LabelAPSsid,APSsid.c_str());
    lv_label_set_text(ui_LabelAPPass,APPass.c_str());
    Refresh_ScreenWifi();

}

void Refresh_ScreenWifi() {
    char apIpText[16];
    char localIpText[16];

    if ((WiFi.getMode() & WIFI_AP) != 0) {
        formatIpAddress(apIpText, sizeof(apIpText), WiFi.softAPIP());
    } else {
        snprintf(apIpText, sizeof(apIpText), "AP inactif");
    }

    if (WiFi.status() == WL_CONNECTED) {
        formatIpAddress(localIpText, sizeof(localIpText), WiFi.localIP());
    } else {
        snprintf(localIpText, sizeof(localIpText), "Not connected");
    }

    setLabelTextIfChanged(ui_LabelIP, apIpText);
    setLabelTextIfChanged(ui_LabelLocalIP, localIpText);
}



void SwitchAPEnable(lv_event_t * e){

    if(lv_obj_has_state(ui_SwitchApEnabled, LV_STATE_CHECKED)) {
        Serial.println("démarrage du mode AP");
        StartAccessPoint();

        IPAddress ip = WiFi.softAPIP();
        Serial.print("IP du point d'accès : ");
        Serial.println(ip);


        lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(0, 200, 0),0);
        lv_label_set_text(ui_LabelWifiApEnabled,"ON");

    } else {
        Serial.println("désactivation du mode AP");
        ConnectToWifi();
        lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(200, 0, 0),0);
        lv_label_set_text(ui_LabelWifiApEnabled,"OFF");
    }

    Refresh_ScreenWifi();
}