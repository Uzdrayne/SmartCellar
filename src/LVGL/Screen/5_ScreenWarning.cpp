#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ui.h>
#include "Alarm/AlertManager.h"
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"



/*

ui_WarningSpinner : Spinner pour indiquer que le système est en alerte (rien à faire avec)
ui_WarningSwitchIcon : Switch à faire clignoter en checked et unchecked (il s'agit d'un icone panneau attention à deux états)
ui_WarningLabelAlerte : label pour afficher l'alerte principale (grosse police ce limiter à un seul mot comme "Alerte !")
ui_WarningLabelReason : label pour afficher la raison de l'alerte (3 lignes disponibles)
ui_WarningLabellastMeasure : label pour afficher la dernière mesure disponible ("derniere mesure XX.X°C")
ui_WarningLabelLastTime : label pour afficher le dernier temps de mesure ("Il y a XXX min")

*/


namespace {

constexpr unsigned long WarningReturnDelayMs = 60000UL;
constexpr unsigned long WarningIconBlinkIntervalMs = 500UL;

bool previousPrimaryAlertActive = false;
bool warningIconChecked = false;
unsigned long warningIconLastToggle = 0;

void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
    if (label != nullptr && strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void appendReasonLine(char* text, const size_t textSize, size_t& lineCount,const char* reason) {
                      
    if (lineCount >= 3) {
        return;
    }

    const size_t textLength = strlen(text);
    if (textLength >= textSize - 1) {
        return;
    }

    snprintf(text + textLength, textSize - textLength, "%s%s",
             textLength == 0 ? "" : "\n", reason);
    ++lineCount;
}

void navigateToWarningScreen() {
    _ui_screen_change(
        &ui_ScreenWarning,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenWarning_screen_init
    );
}

}


void Initialize_ScreenWarning() {
    lv_obj_clear_flag(ui_WarningSwitchIcon, LV_OBJ_FLAG_CLICKABLE);
    previousPrimaryAlertActive = false;
    warningIconChecked = false;
    warningIconLastToggle = millis();
    Refresh_ScreenWarning();
}

void Refresh_ScreenWarning() {
    const unsigned long now = millis();
    const AlertSnapshot status = GetAlertSnapshot(now);

    setLabelTextIfChanged(
        ui_WarningLabelAlerte,
        status.AnyAlertActive ? "Alerte !" : "Normal"
    );

    char reasonText[128] = {};
    size_t reasonLineCount = 0;
    if (status.NanoLinkLost) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "Liaison avec le Nano perdue");
    } else if (status.MeasurementsUnavailable) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "Mesures indisponibles");
    }
    if (status.HighTemperatureActive) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "Temperature trop elevee");
    }
    if (status.NanoFallback) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "Mode secours du Nano");
    }
    if (status.WifiDisconnected) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "Wi-Fi indisponible");
    }
    if (status.MqttDisconnected) {
        appendReasonLine(reasonText, sizeof(reasonText), reasonLineCount,
                         "MQTT indisponible");
    }
    if (reasonText[0] == '\0') {
        snprintf(reasonText, sizeof(reasonText),
                 status.AnyAlertActive ? "Aucune alerte active" : "Retour a la normale");
    }
    setLabelTextIfChanged(ui_WarningLabelReason, reasonText);

    char measurementText[48];
    char ageText[40];
    if (status.HasLastMeasurement && isfinite(status.LastValidTemperatureC)) {
        snprintf(measurementText, sizeof(measurementText),
                 "Derniere mesure %.1f°C",
                 static_cast<double>(status.LastValidTemperatureC));
        if (status.LastMeasurementAgeSeconds < 60UL) {
            snprintf(ageText, sizeof(ageText), "Il y a moins d'1 min");
        } else {
            snprintf(ageText, sizeof(ageText), "Il y a %lu min",
                     status.LastMeasurementAgeSeconds / 60UL);
        }
    } else {
        snprintf(measurementText, sizeof(measurementText), "Aucune mesure connue");
        snprintf(ageText, sizeof(ageText), "En attente de donnees");
    }
    setLabelTextIfChanged(ui_WarningLabellastMeasure, measurementText);
    setLabelTextIfChanged(ui_WarningLabelLastTime, ageText);

    if (!status.AnyAlertActive) {
        warningIconChecked = false;
        warningIconLastToggle = now;
    } else if (now - warningIconLastToggle >= WarningIconBlinkIntervalMs) {
        warningIconChecked = !warningIconChecked;
        warningIconLastToggle = now;
    }
    set_switch_state(ui_WarningSwitchIcon, warningIconChecked);
}

void Handle_ScreenWarningNavigation(const unsigned long now) {
    if (ui_ScreenWarning == nullptr) {
        return;
    }

    const AlertSnapshot status = GetAlertSnapshot(now);
    const bool warningScreenActive = lv_scr_act() == ui_ScreenWarning;

    if (status.PrimaryAlertActive && !previousPrimaryAlertActive &&
        !warningScreenActive) {
        navigateToWarningScreen();
    }
    previousPrimaryAlertActive = status.PrimaryAlertActive;

    if (status.PrimaryAlertActive &&
        lv_scr_act() != ui_ScreenWarning &&
        now - LastTouch >= WarningReturnDelayMs) {
        navigateToWarningScreen();
    }
}

void ScreenWarningGestDown(lv_event_t *e) {

    _ui_screen_change(
        &ui_ScreenHome,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHome_screen_init
    );        
}

void ScreenWarningGestUp(lv_event_t *e) {

    _ui_screen_change(
        &ui_ScreenHome,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHome_screen_init
    );        
}
