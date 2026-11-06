#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "LVGL/Lvgl_Global.h"

#include "Controller/ConfigurationLimits.h"

/*
ui_FanSwitchActive : switch pour activer ou désactiver le ventilateur

ui_FanLabelOnTime : label affichant le temps de fonctionnement du ventilateur
ui_FanButtonOnMinus : Bouton pour diminuer le temps de fonctionnement du ventilateur
ui_FanButtonOnPlus : Bouton pour augmenter le temps de fonctionnement du ventilateur

ui_FanLabelOffTime : label affichant le temps d'arrêt du ventilateur
ui_FanButtonOffMinus : Bouton pour diminuer le temps d'arrêt du ventilateur
ui_FanButtonOffPlus : Bouton pour augmenter le temps d'arrêt du ventilateur

ui_FanButtonApplyChanges : Bouton pour appliquer les modifications du ventilateur
ui_FanButtonDiscardChanges : Bouton pour annuler les modifications du ventilateur

ui_FanSwitchLock : switch pour verrouiller ou déverrouiller le contrôle du ventilateur
ui_FanSwitchFanState : switch indiquant l'état actuel du ventilateur

*/


namespace {

    bool fanDraftInitialized = false;
    bool fanDraftEnabled = false;
    bool originalFanEnabled = false;
    unsigned int fanDraftOnMinutes = FanDurationMinMinutes;
    unsigned int fanDraftOffMinutes = FanDurationMinMinutes;
    unsigned int originalFanOnMinutes = FanDurationMinMinutes;
    unsigned int originalFanOffMinutes = FanDurationMinMinutes;
    bool fanScreenLocked = true;

    void LoadFanDraft(const StrctDataController& data) {
        fanDraftEnabled = data.FanEnabled;
        fanDraftOnMinutes = data.FanOnMinutes;
        fanDraftOffMinutes = data.FanOffMinutes;
        originalFanEnabled = fanDraftEnabled;
        originalFanOnMinutes = fanDraftOnMinutes;
        originalFanOffMinutes = fanDraftOffMinutes;
        fanDraftInitialized = true;
    }

    bool FanChangesPending() {
        return fanDraftEnabled != originalFanEnabled ||
            fanDraftOnMinutes != originalFanOnMinutes ||
            fanDraftOffMinutes != originalFanOffMinutes;
    }

    void RefreshFanDraftDisplay() {
        set_switch_state(ui_FanSwitchActive, fanDraftEnabled);

        char text[16];
        snprintf(text, sizeof(text), "%02u", fanDraftOnMinutes);
        lv_label_set_text(ui_FanLabelOnTime, text);

        snprintf(text, sizeof(text), "%02u", fanDraftOffMinutes);
        lv_label_set_text(ui_FanLabelOffTime, text);
    }

    void UpdateFanControlStates() {
        const bool editable = !fanScreenLocked;
        const bool changesPending = editable && FanChangesPending();

        if (editable) {
            lv_obj_clear_state(ui_FanSwitchActive, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(ui_FanSwitchActive, LV_STATE_DISABLED);
        }

        const lv_imgbtn_state_t imgButtonState = editable
            ? LV_IMGBTN_STATE_RELEASED
            : LV_IMGBTN_STATE_DISABLED;
        lv_imgbtn_set_state(ui_FanButtonOnMinus, imgButtonState);
        lv_imgbtn_set_state(ui_FanButtonOnPlus, imgButtonState);
        lv_imgbtn_set_state(ui_FanButtonOffMinus, imgButtonState);
        lv_imgbtn_set_state(ui_FanButtonOffPlus, imgButtonState);

        const lv_imgbtn_state_t actionButtonState = changesPending
            ? LV_IMGBTN_STATE_RELEASED
            : LV_IMGBTN_STATE_DISABLED;
        lv_imgbtn_set_state(ui_FanButtonApplyChanges, actionButtonState);
        lv_imgbtn_set_state(ui_FanButtonDiscardChanges, actionButtonState);
    }

    void ChangeFanDuration(unsigned int& duration, bool increase) {
        if (increase) {
            if (duration < FanDurationMaxMinutes) {
                ++duration;
            }
        } else if (duration > FanDurationMinMinutes) {
            --duration;
        }

        RefreshFanDraftDisplay();
        UpdateFanControlStates();
    }

    void ApplyFanLock() {
        fanScreenLocked =
            !lv_obj_has_state(ui_FanSwitchLock, LV_STATE_CHECKED);
        LoadFanDraft(GetControllerSnapshot());
        RefreshFanDraftDisplay();
        UpdateFanControlStates();
    }

    void LockFanScreenAfterAction() {
        lv_obj_clear_state(ui_FanSwitchLock, LV_STATE_CHECKED);
        ApplyFanLock();
    }
}

void Initialize_ScreenFan() {
    Refresh_ScreenFan();
}

void Refresh_ScreenFan() {
    const StrctDataController data = GetControllerSnapshot();
    const bool lockedFromSwitch =
        !lv_obj_has_state(ui_FanSwitchLock, LV_STATE_CHECKED);

    if (!fanDraftInitialized || lockedFromSwitch != fanScreenLocked ||
        lockedFromSwitch) {
        fanScreenLocked = lockedFromSwitch;
        LoadFanDraft(data);
    }

    set_switch_state(ui_FanSwitchFanState, data.FanState);
    lv_obj_add_state(ui_FanSwitchFanState, LV_STATE_DISABLED);

    RefreshFanDraftDisplay();
    UpdateFanControlStates();
}


// Event quand le switch d'activation du ventilateur est cliqué
void FanSwitchActiveClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }

    fanDraftEnabled =
        lv_obj_has_state(ui_FanSwitchActive, LV_STATE_CHECKED);
    UpdateFanControlStates();
}

// Event quand le bouton pour diminuer le temps de fonctionnement du ventilateur est cliqué
void FanButtonOnMinusClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }
    ChangeFanDuration(fanDraftOnMinutes, false);
}

// Event quand le bouton pour augmenter le temps de fonctionnement du ventilateur est cliqué
void FanButtonOnPlusClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }
    ChangeFanDuration(fanDraftOnMinutes, true);
}

// Event quand le bouton pour diminuer le temps d'arrêt du ventilateur est cliqué
void FanButtonOffMinusClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }
    ChangeFanDuration(fanDraftOffMinutes, false);
}

// Event quand le bouton pour augmenter le temps d'arrêt du ventilateur est cliqué
void FanButtonOffPlusClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }
    ChangeFanDuration(fanDraftOffMinutes, true);
}

// Event quand le bouton pour appliquer les modifications du ventilateur est cliqué
void FanButtonApplyChangesClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED ||
        fanScreenLocked || !FanChangesPending()) {
        return;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataController.FanEnabled = fanDraftEnabled;
    DataController.FanOnMinutes = fanDraftOnMinutes;
    DataController.FanOffMinutes = fanDraftOffMinutes;
    xSemaphoreGive(DataMutex);

    Save_FanSettings();
    LockFanScreenAfterAction();
}
// Event quand le bouton pour annuler les modifications du ventilateur est cliqué
void FanButtonDiscardChangesClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || fanScreenLocked) {
        return;
    }

    LockFanScreenAfterAction();
}

// Event quand le switch de verrouillage du ventilateur est cliqué
void FanSwitchLockClicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ApplyFanLock();
}


// Gestion du geste vers le haut sur le screen Fan
void ScreenFanGestUp(lv_event_t *e) {
    if (!fanScreenLocked) {
        return;
    }

    _ui_screen_change(
        &ui_ScreenWifi,
        LV_SCR_LOAD_ANIM_FADE_ON,
        300,
        0,
        &ui_ScreenWifi_screen_init
    );        
}
// Gestion du geste vers le bas sur le screen Fan
void ScreenFanGestDown(lv_event_t *e) {
    if (!fanScreenLocked) {
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
