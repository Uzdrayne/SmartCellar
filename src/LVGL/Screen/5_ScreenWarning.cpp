#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <ui.h>
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



}


void Initialize_ScreenWarning() {


    Refresh_ScreenWarning();
}

void Refresh_ScreenWarning() {

}