#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Controller/ConfigurationLimits.h"
#include "Controller/ControllerValidation.h"



/*
ui_HumArcSetHum : Arc pour régler l'humidité souhaitée

ui_HumLabelSetHum : Label affichant la consigne d'humidité (XX sans unité)
ui_HumLabelCurrentHum : Label affichant l'humidité actuelle (XX sans unité)

ui_HumSwitchHumState : Switch indiquant l'état de l'humidificateur (ON/OFF)
ui_HumSwitchDehumState : Switch indiquant l'état du déshumidificateur (ON/OFF)

ui_HumButtonOption : bouton pour aller au screen option humidité

ui_HumSwitchLock : Switch pour verrouiller le screen humidité
*/

namespace {

    bool measurementsAvailable = false;
    bool humSetpointBeingEdited = false;
    bool humScreenLocked = true;

    void ApplyHumidityLock() {
        humScreenLocked =
            !lv_obj_has_state(ui_HumSwitchLock, LV_STATE_CHECKED);

        if (humScreenLocked) {
            humSetpointBeingEdited = false;
            lv_obj_add_state(ui_HumArcSetHum, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(ui_HumArcSetHum, LV_STATE_DISABLED);
        }

        lv_imgbtn_set_state(
            ui_HumButtonOption,
            humScreenLocked ? LV_IMGBTN_STATE_DISABLED : LV_IMGBTN_STATE_RELEASED
        );
    }

}

// Initialise le screen humidité
void Initialize_ScreenHum() {

    //réccupérer les données actuelles du contrôleur
    const StrctDataController data = GetControllerSnapshot();

    // Activer le hit test (évite les problèmes de détection des clics sur l'arc)
    lv_obj_add_flag(ui_HumArcSetHum, LV_OBJ_FLAG_ADV_HITTEST);


    //valeur minimale et maximale de l'arc en fonction des limites de consigne
    lv_arc_set_range(
        ui_HumArcSetHum,
        static_cast<int16_t>(MinHumiditySetpointPercent),
        static_cast<int16_t>(MaxHumiditySetpointPercent)
    );

    lv_arc_set_value(
        ui_HumArcSetHum,
        static_cast<int16_t>(data.SetHum + 0.5f)
    );


    ApplyHumidityLock();
    Refresh_ScreenHum();
}

// Rafraîchit l'affichage du screen humidité en fonction des données actuelles du contrôleur
void Refresh_ScreenHum() {

    const StrctDataController data = GetControllerSnapshot();
    measurementsAvailable = HasValidControllerMeasurements(data);

    if (!humSetpointBeingEdited) {
        lv_arc_set_value(
            ui_HumArcSetHum,
            static_cast<int16_t>(data.SetHum + 0.5f)
        );

        char setpointText[16];
        snprintf(setpointText, sizeof(setpointText), "%02d", static_cast<int16_t>(data.SetHum + 0.5f));
        lv_label_set_text(ui_HumLabelSetHum, setpointText);
    }


    //si mesure valable, afficher l'humidité actuelle sinon afficher "--"
    if (measurementsAvailable) {
        const float humidity = data.Humidity + (data.HumOffsetEnabled ? data.HumOffset : 0.0f);
        char text[16];
        snprintf(text, sizeof(text), "%02.0f", humidity);
        lv_label_set_text(ui_HumLabelCurrentHum, text);
    } else {
        lv_label_set_text(ui_HumLabelCurrentHum, "--");
    }


    //mettre à jour l'état des switches d'humidification et déshumidification
    set_switch_state(
        ui_HumSwitchHumState,
        measurementsAvailable && data.HumState
    );
    set_switch_state(
        ui_HumSwitchDehumState,
        measurementsAvailable && data.DehumState
    );





}

// Gestion de l'événement de changement de valeur de l'arc d'humidité
void HumArcSetHumValueChanged(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || humScreenLocked) {
        return;
    }

    humSetpointBeingEdited = true;

    const int16_t valeurArc = lv_arc_get_value(lv_event_get_target(e));
    const float consigneHum = valeurArc;

    char text[16];
    snprintf(text, sizeof(text), "%02.0f", consigneHum);
    lv_label_set_text(ui_HumLabelSetHum, text);
}

// Gestion de la libération de l'arc d'humidité
void HumArcSetHumReleased(lv_event_t *e) {
    if (!humSetpointBeingEdited || humScreenLocked) {
        return;
    }
    humSetpointBeingEdited = false;

    const int16_t valeurArc = lv_arc_get_value(lv_event_get_target(e));
    const float consigneHum = valeurArc;
    if (!IsValidHumiditySetpoint(consigneHum)) {
        Serial.printf("LVGL : consigne humidite invalide: %.2f\n", consigneHum);
        return;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const bool changed = DataController.SetHum != consigneHum;
    DataController.SetHum = consigneHum;
    xSemaphoreGive(DataMutex);

    if (changed && !Save_SetTemp_and_SetHum()) {
        Serial.println("LVGL : echec de sauvegarde de la consigne humidite.");
    }
}

// Gestion de la perte de pression sur l'arc d'humidité
void HumArcSetHumPressLost(lv_event_t *e) {
    humSetpointBeingEdited = false;
}

// Gestion du clic sur le bouton d'option de l'écran humidité
void HumButtonOptionClicked(lv_event_t *e) {
    if (humScreenLocked) {
        return;
    }

    lv_obj_clear_state(ui_HumSwitchLock, LV_STATE_CHECKED);
    ApplyHumidityLock();

    _ui_screen_change(
        &ui_ScreenHome,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenHome_screen_init
    );
}

// Gestion du clic sur le bouton de verrouillage de l'écran humidité
void HumSwitchLockClicked(lv_event_t *e) {
    ApplyHumidityLock();
}

// Gestion du geste vers le haut sur le screen humidité
void ScreenHumGestUp(lv_event_t *e) {
    if (!humScreenLocked) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenTemp,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenTemp_screen_init
    );
}

// Gestion du geste vers le bas sur le screen humidité
void ScreenHumGestDown(lv_event_t *e) {
    if (!humScreenLocked) {
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
