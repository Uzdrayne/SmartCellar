#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <ui.h>
#include <string.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Wifi/my_wifi.h"
#include "ServerWeb/WebSecurity.h"


/*

ui_WifiSpinner : Spinner pour faire jolie

ui_WifiSwitchApEnabled : Switch pour activer ou désactiver le mode AP du WiFi

ui_WifiLabelApSsid : label pour afficher le SSID du point d'accès WiFi
ui_WifiLabelApPass : label pour afficher le mot de passe du point d'accès WiFi
ui_WifiLabelApIP : label pour afficher l'adresse IP du point d'accès WiFi

ui_WifiLabelLocalIp : label pour afficher l'adresse IP locale du WiFi


*/

namespace {

lv_obj_t* adminResetButton = nullptr;
lv_obj_t* adminResetButtonLabel = nullptr;
bool adminResetConfirmationPending = false;
bool adminResetFailed = false;
unsigned long adminResetRequestedAt = 0;
constexpr unsigned long AdminResetConfirmationWindowMs = 5000;

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

void handleAdminResetButton(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !HasWebAdminCredential()) {
        return;
    }

    const unsigned long now = millis();
    if (!adminResetConfirmationPending ||
        now - adminResetRequestedAt > AdminResetConfirmationWindowMs) {
        adminResetConfirmationPending = true;
        adminResetFailed = false;
        adminResetRequestedAt = now;
        lv_label_set_text(adminResetButtonLabel, "Confirmer le reset");
        return;
    }

    adminResetConfirmationPending = false;
    if (ResetWebAdminCredential()) {
        Serial.println("[WebSecurity] Compte Web réinitialisé depuis l'écran");
        lv_label_set_text(adminResetButtonLabel, "Accès à reconfigurer via AP");
    } else {
        adminResetFailed = true;
        lv_label_set_text(adminResetButtonLabel, "Échec de réinitialisation");
    }
}

void createAdminResetButton() {
    if (adminResetButton != nullptr) {
        return;
    }

    adminResetButton = lv_btn_create(ui_ScreenWifi);
    lv_obj_set_size(adminResetButton, 200, 36);
    lv_obj_align(adminResetButton, LV_ALIGN_BOTTOM_MID, 0, -4);
    adminResetButtonLabel = lv_label_create(adminResetButton);
    lv_label_set_text(adminResetButtonLabel, "Reinitialiser acces Web");
    lv_obj_center(adminResetButtonLabel);
    lv_obj_add_event_cb(
        adminResetButton, handleAdminResetButton, LV_EVENT_CLICKED, nullptr);
    if (!HasWebAdminCredential()) {
        lv_obj_add_state(adminResetButton, LV_STATE_DISABLED);
        lv_label_set_text(adminResetButtonLabel, "Acces a configurer via AP");
    }
}

}

bool StateSwitchAP = false;

void ScreenWifi_Loaded(lv_event_t * e){

    StateSwitchAP = (WiFi.getMode() & WIFI_AP) != 0;

   // set_switch_state(ui_SwitchApEnabled,StateSwitchAP);

    const lv_color_t statusColor = StateSwitchAP
        ? lv_color_make(0, 200, 0)
        : lv_color_make(200, 0, 0);
   // lv_obj_set_style_text_color(ui_LabelWifiApEnabled, statusColor, 0);
   // lv_label_set_text(ui_LabelWifiApEnabled, StateSwitchAP ? "ON" : "OFF");



    createAdminResetButton();
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


    lv_label_set_text(ui_WifiLabelApSsid,APSsid.c_str()); //SSID
    lv_label_set_text(ui_WifiLabelApPass,APPass.c_str()); //Password
    lv_label_set_text(ui_WifiLabelApIP,apIpText); //IP

    lv_label_set_text(ui_WifiLabelLocalIp, localIpText); //Local IP


    if (adminResetButton != nullptr) {
        if (adminResetConfirmationPending &&
            millis() - adminResetRequestedAt > AdminResetConfirmationWindowMs) {
            adminResetConfirmationPending = false;
        }

        if (HasWebAdminCredential()) {
            lv_obj_clear_state(adminResetButton, LV_STATE_DISABLED);
            if (!adminResetConfirmationPending && !adminResetFailed) {
                lv_label_set_text(adminResetButtonLabel, "Reinitialiser acces Web");
            }
        } else {
            lv_obj_add_state(adminResetButton, LV_STATE_DISABLED);
            if (!adminResetFailed) {
                lv_label_set_text(adminResetButtonLabel, "Acces a configurer via AP");
            }
        }
    }
}



void SwitchAPEnable(lv_event_t * e){

    
    if(lv_obj_has_state(ui_WifiSwitchApEnabled, LV_STATE_CHECKED)) {
        Serial.println("démarrage du mode AP");
        StartAccessPoint();

        IPAddress ip = WiFi.softAPIP();
        Serial.print("IP du point d'accès : ");
        Serial.println(ip);


        //lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(0, 200, 0),0);
        //lv_label_set_text(ui_LabelWifiApEnabled,"ON");

    } else {
        Serial.println("désactivation du mode AP");
        ConnectToWifi();
        //lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(200, 0, 0),0);
       // lv_label_set_text(ui_LabelWifiApEnabled,"OFF");
    }

    Refresh_ScreenWifi();
}