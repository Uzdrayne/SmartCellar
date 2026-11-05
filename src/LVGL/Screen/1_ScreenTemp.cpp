#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Controller/ConfigurationLimits.h"
#include "Controller/ControllerValidation.h"



/*
ui_TempArcSetTemp : Arc pour régler la température souhaitée

ui_TempLabelSetTemp : Label affichant la consigne detempérature (XX.X sans unité)
ui_TempLabelCurrentTemp : Label affichant la température actuelle (XX.X sans unité)

ui_TempSwitchHeatState : Switch indiquant l'état du chauffage (ON/OFF)
ui_TempSwitchColdState : Switch indiquant l'état de la climatisation (ON/OFF)

ui_TempButtonOption : bouton pour aller au screen option temperature

ui_TempSwitchLock : Switch pour verrouiller le screen temperature
*/

namespace {

    bool measurementsAvailable = false;
    bool tempSetpointBeingEdited = false;
    bool tempScreenLocked = true;

    void ApplyTemperatureLock() {
        tempScreenLocked =
            !lv_obj_has_state(ui_TempSwitchLock, LV_STATE_CHECKED);

        if (tempScreenLocked) {
            tempSetpointBeingEdited = false;
            lv_obj_add_state(ui_TempArcSetTemp, LV_STATE_DISABLED);
            lv_obj_add_state(ui_TempButtonOption, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(ui_TempArcSetTemp, LV_STATE_DISABLED);
            lv_obj_clear_state(ui_TempButtonOption, LV_STATE_DISABLED);
        }
    }

}

// Initialise le screen temperature
void Initialize_ScreenTemp() {

    //réccupérer les données actuelles du contrôleur
    const StrctDataController data = GetControllerSnapshot();

    // Activer le hit test (évite les problèmes de détection des clics sur l'arc)
    lv_obj_add_flag(ui_TempArcSetTemp, LV_OBJ_FLAG_ADV_HITTEST);


    //valeur minimale et maximale de l'arc en fonction des limites de consigne
    lv_arc_set_range(
        ui_TempArcSetTemp,
        static_cast<int16_t>(MinTemperatureSetpointC * 2.0f), // 10 = 5 °C
        static_cast<int16_t>(MaxTemperatureSetpointC * 2.0f)  // 60 = 30 °C
    );

    lv_arc_set_value(
        ui_TempArcSetTemp,
        static_cast<int16_t>(data.SetTemp * 2.0f)
    );

    char text[16];
    snprintf(text, sizeof(text), "%04.1f", data.SetTemp);
    lv_label_set_text(ui_TempLabelSetTemp, text);

    ApplyTemperatureLock();
    Refresh_ScreenTemp();
}

// Rafraîchit l'affichage du screen temperature en fonction des données actuelles du contrôleur
void Refresh_ScreenTemp() {

    const StrctDataController data = GetControllerSnapshot();
    measurementsAvailable = HasValidControllerMeasurements(data);

    if (!tempSetpointBeingEdited) {
        lv_arc_set_value(
            ui_TempArcSetTemp,
            static_cast<int16_t>(data.SetTemp * 2.0f)
        );

        char setpointText[16];
        snprintf(setpointText, sizeof(setpointText), "%04.1f", data.SetTemp);
        lv_label_set_text(ui_TempLabelSetTemp, setpointText);
    }


    //si mesure valable, afficher la température actuelle sinon afficher "--.-"
    if (measurementsAvailable) {
        const float temperature = data.Temperature + (data.TempOffsetEnabled ? data.TempOffset : 0.0f);
        char text[16];
        snprintf(text, sizeof(text), "%04.1f", temperature);
        lv_label_set_text(ui_TempLabelCurrentTemp, text);
    } else {
        lv_label_set_text(ui_TempLabelCurrentTemp, "--.-");
    }
    

    //mettre à jour l'état des switches de chauffage et climatisation
    set_switch_state(
        ui_TempSwitchHeatState,
        measurementsAvailable && data.HeatState
    );
    set_switch_state(
        ui_TempSwitchColdState,
        measurementsAvailable && data.ColdState
    );





}

// Gestion de l'événement de changement de valeur de l'arc de température
void TempArcSetTempValueChanged(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || tempScreenLocked) {
        return;
    }

    tempSetpointBeingEdited = true;

    const int16_t valeurArc = lv_arc_get_value(lv_event_get_target(e));
    const float consigneC = valeurArc / 2.0f;

    char text[16];
    snprintf(text, sizeof(text), "%04.1f", consigneC);
    lv_label_set_text(ui_TempLabelSetTemp, text);
}

// Gestion de la libération de l'arc de température
void TempArcSetTempReleased(lv_event_t *e) {
    if (!tempSetpointBeingEdited || tempScreenLocked) {
        return;
    }
    tempSetpointBeingEdited = false;

    const int16_t valeurArc = lv_arc_get_value(lv_event_get_target(e));
    const float consigneC = valeurArc / 2.0f;
    if (!IsValidTemperatureSetpoint(consigneC)) {
        Serial.printf("LVGL : consigne temperature invalide: %.2f\n", consigneC);
        return;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const bool changed = DataController.SetTemp != consigneC;
    DataController.SetTemp = consigneC;
    xSemaphoreGive(DataMutex);

    if (changed && !Save_SetTemp_and_SetHum()) {
        Serial.println("LVGL : echec de sauvegarde de la consigne temperature.");
    }
}

// Gestion de la perte de pression sur l'arc de température
void TempArcSetTempPressLost(lv_event_t *e) {
    tempSetpointBeingEdited = false;
}

// Gestion du clic sur le bouton d'option de l'écran température
void TempButtonOptionClicked(lv_event_t *e) {
    if (tempScreenLocked) {
        return;
    }

    lv_obj_clear_state(ui_TempSwitchLock, LV_STATE_CHECKED);
    ApplyTemperatureLock();

    _ui_screen_change(
        &ui_ScreenHome,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHome_screen_init
    );   
}

// Gestion du clic sur le bouton de verrouillage de l'écran température
void TempSwitchLockClicked(lv_event_t *e) {
    ApplyTemperatureLock();
}

// Gestion du geste vers le haut sur le screen temperature
void ScreenTempGestUp(lv_event_t *e) {
    if (!tempScreenLocked) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenHum,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHum_screen_init
    );    
}

// Gestion du geste vers le bas sur le screen temperature
void ScreenTempGestDown(lv_event_t *e) {
    if (!tempScreenLocked) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenHome,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHome_screen_init
    );    
}