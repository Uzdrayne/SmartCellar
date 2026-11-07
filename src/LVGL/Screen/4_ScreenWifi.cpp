#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <ui.h>
#include <string.h>
#include "Alarm/AlertManager.h"
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

ui_WifiSwitchLock : switch pour verrouiller le screen Wifi


ui_WifiButtonReset : bouton pour réinitialiser l'accès admin Web
ui_WifiLabelBtnReset : label pour afficher le texte du bouton de réinitialisation de l'admin Web
*/

namespace {

enum class AdminResetFeedback {
    None,
    Succeeded,
    Failed
};

lv_obj_t* adminResetMessageBox = nullptr;
AdminResetFeedback adminResetFeedback = AdminResetFeedback::None;
bool wifiScreenLocked = true;
bool accessPointEnabled = false;

const char* AdminResetMessageBoxButtons[] = {"Oui", "Non", ""};

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

void UpdateWifiControlStates() {
    if (wifiScreenLocked) {
        lv_obj_add_state(ui_WifiSwitchApEnabled, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(ui_WifiSwitchApEnabled, LV_STATE_DISABLED);
    }

    if (wifiScreenLocked || !HasWebAdminCredential()) {
        lv_obj_add_state(ui_WifiButtonReset, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(ui_WifiButtonReset, LV_STATE_DISABLED);
    }
}

void ApplyWifiLock() {
    wifiScreenLocked =
        !lv_obj_has_state(ui_WifiSwitchLock, LV_STATE_CHECKED);
    UpdateWifiControlStates();
}

void handleAdminResetConfirmation(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
        adminResetMessageBox == nullptr) {
        return;
    }

    const uint16_t activeButton =
        lv_msgbox_get_active_btn(adminResetMessageBox);
    if (activeButton == 0) {
        if (ResetWebAdminCredential()) {
            adminResetFeedback = AdminResetFeedback::Succeeded;
            Serial.println("[WebSecurity] Compte Web réinitialisé depuis l'écran");
        } else {
            adminResetFeedback = AdminResetFeedback::Failed;
            Serial.println("[WebSecurity] Echec de réinitialisation du compte Web");
        }
    }

    lv_msgbox_close_async(adminResetMessageBox);
    adminResetMessageBox = nullptr;
    Refresh_ScreenWifi();
}

}


void Initialize_ScreenWifi() {
    lv_obj_clear_state(ui_WifiSwitchLock, LV_STATE_CHECKED);
    adminResetFeedback = AdminResetFeedback::None;
    ApplyWifiLock();
    Refresh_ScreenWifi();
}

void Refresh_ScreenWifi() {
    accessPointEnabled = (WiFi.getMode() & WIFI_AP) != 0;
    set_switch_state(ui_WifiSwitchApEnabled, accessPointEnabled);

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

    if (adminResetFeedback == AdminResetFeedback::Succeeded &&
        HasWebAdminCredential()) {
        adminResetFeedback = AdminResetFeedback::None;
    }

    if (adminResetFeedback == AdminResetFeedback::None) {
        setLabelTextIfChanged(
            ui_WifiLabelBtnReset,
            HasWebAdminCredential()
                ? "Reset Mdp"
                : "Config via AP"
        );
    } else if (adminResetFeedback == AdminResetFeedback::Succeeded) {
        setLabelTextIfChanged(
            ui_WifiLabelBtnReset,
            "Config via AP"
        );
    } else {
        setLabelTextIfChanged(
            ui_WifiLabelBtnReset,
            "Reset Erreur"
        );
    }

    UpdateWifiControlStates();
}


// Gestion du clic sur le switch d'activation du mode AP
void WifiSwitchAPEnableClicked(lv_event_t * e){
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (wifiScreenLocked) {
        set_switch_state(ui_WifiSwitchApEnabled, accessPointEnabled);
        return;
    }

    if(lv_obj_has_state(ui_WifiSwitchApEnabled, LV_STATE_CHECKED)) {
        Serial.println("démarrage du mode AP");
        StartAccessPoint();

        IPAddress ip = WiFi.softAPIP();
        Serial.print("IP du point d'accès : ");
        Serial.println(ip);


    } else {
        Serial.println("désactivation du mode AP");
        ConnectToWifi();
        //lv_obj_set_style_text_color(ui_LabelWifiApEnabled, lv_color_make(200, 0, 0),0);
       // lv_label_set_text(ui_LabelWifiApEnabled,"OFF");
    }

    Refresh_ScreenWifi();
}


// Gestion du clic sur le bouton de réinitialisation de l'admin
void WifiButtonResetClicked(lv_event_t * e){
    if (lv_event_get_code(e) != LV_EVENT_CLICKED ||
        wifiScreenLocked || !HasWebAdminCredential() ||
        adminResetMessageBox != nullptr) {
        return;
    }

    adminResetFeedback = AdminResetFeedback::None;
    adminResetMessageBox = lv_msgbox_create(
        nullptr,
        "Confirmation",
        "Reinitialiser l'acces administrateur Web ?",
        AdminResetMessageBoxButtons,
        false
    );
    if (adminResetMessageBox == nullptr) {
        Serial.println("[WebSecurity] Impossible de creer la confirmation de reset");
        return;
    }

    lv_obj_add_event_cb(
        adminResetMessageBox,
        handleAdminResetConfirmation,
        LV_EVENT_VALUE_CHANGED,
        nullptr
    );
    lv_obj_center(adminResetMessageBox);
}

// Gestion du clic sur le switch de verrouillage du screen Wifi
void WifiSwitchLockClicked(lv_event_t * e){
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ApplyWifiLock();
    Refresh_ScreenWifi();
}

// Gestion du geste vers le bas sur le screen Fan
void ScreenWifiGestDown(lv_event_t *e) {
    if (!wifiScreenLocked) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenFan,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenFan_screen_init
    );        
}


// Gestion du geste vers le haut sur le screen Wifi
void ScreenWifiGestUp(lv_event_t *e) {
    if (!GetAlertSnapshot(millis()).AnyAlertActive) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenWarning,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenWarning_screen_init
    );
}