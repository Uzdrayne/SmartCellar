#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
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

}

// Initialise le screen humidité
void Initialize_ScreenHum() {

}

// Rafraîchit l'affichage du screen humidité en fonction des données actuelles du contrôleur
void Refresh_ScreenHum() {

}


// Gestion de l'événement de changement de valeur de l'arc d'humidité
void HumArcSetHumValueChanged(lv_event_t *e) {

}

// Gestion de la libération de l'arc de température
void HumArcSetHumReleased(lv_event_t *e) {

}

// Gestion de la libération de l'arc d'humidité
void HumArcSetHumPressLost(lv_event_t *e) {

}

// Gestion du clic sur le bouton d'option de l'écran humidité
void HumButtonOptionClicked(lv_event_t *e) {

}

// Gestion du clic sur le bouton de verrouillage de l'écran humidité
void HumSwitchLockClicked(lv_event_t *e) {

}   

// Gestion du geste vers le haut sur le screen humidité
void ScreenHumGestUp(lv_event_t *e) {

}

// Gestion du geste vers le bas sur le screen humidité
void ScreenHumGestDown(lv_event_t *e) {

}   