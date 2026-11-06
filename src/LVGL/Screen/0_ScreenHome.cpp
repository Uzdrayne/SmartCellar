#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

/*

ui_HomeSpinner : Spinner de chargement (indique que le soft ne freeze pas)

ui_HomeSwitchWifiState : état de la connexion WiFi (OK/NOK)
ui_HomeSwitchSerialState : état de la connexion série avec la platine relais (OK/NOK)
ui_HomeSwitchMqttState : état de la connexion MQTT (OK/NOK)

ui_HomeLabelDateTime : Afficher la date et l'heure actuelles (JJ/MM/AAAA HH:MM)

ui_HomeLabelCurrentTemp : Afficher temp actuelle (XX.X sans unité)
ui_HomeLabelCurrentHum : Afficher humidité actuelle (XX sans unité)

ui_HomeLabelConsignes : Afficher les consignes de température et d'humidité ("Cible XX.X°C / YY%")

ui_HomeSwitchHeatState : état du chauffage (ON/OFF)
ui_HomeSwitchColdState : état de la climatisation (ON/OFF)
ui_HomeSwitchHumState : état de l'humidificateur (ON/OFF)
ui_HomeSwitchDehumState : état du déshumidificateur (ON/OFF)
ui_HomeSwitchFanState : état du ventilateur (ON/OFF)
*/




namespace {

  void refreshHomeDateLabel() {
      constexpr time_t minimumValidEpoch = 1577836800; // 2020-01-01 UTC
      const time_t now = time(nullptr);
      if (now < minimumValidEpoch) {
          lv_label_set_text(ui_HomeLabelDateTime, "Attente NTP...");
          return;
      }

      struct tm localTime;
      char dateText[32];
      if (localtime_r(&now, &localTime) == nullptr ||
          strftime(dateText, sizeof(dateText), "%d/%m/%Y %H:%M", &localTime) == 0) {
          lv_label_set_text(ui_HomeLabelDateTime, "Heure indisponible");
          return;
      }

      lv_label_set_text(ui_HomeLabelDateTime, dateText);
  }


}


void Initialize_ScreenHome() {
  Refresh_ScreenHome();
}


void Refresh_ScreenHome(){
  const StrctDataController localData = GetControllerSnapshot();

  //wifi
  set_switch_state(ui_HomeSwitchWifiState,WiFi.status() == WL_CONNECTED);

  //serial avec la platine relais (arduino nano)
  const bool sensorReady = HasValidControllerMeasurements(localData);
  set_switch_state(ui_HomeSwitchSerialState, sensorReady);

  //Mqtt
  set_switch_state(ui_HomeSwitchMqttState, mqtt.connected());
    
  //actualiser la date et l'heure
  refreshHomeDateLabel();


  //Actualisation des valeurs du capteur et consigne
  if(sensorReady) {

    const float correctedTemperature = localData.Temperature + (localData.TempOffsetEnabled ? localData.TempOffset : 0.0f);
    const float correctedHumidity = localData.Humidity + (localData.HumOffsetEnabled ? localData.HumOffset : 0.0f);

    sprintf(bufftext,"%04.1f",correctedTemperature);
    lv_label_set_text(ui_HomeLabelCurrentTemp, bufftext);

    sprintf(bufftext,"%02.0f",correctedHumidity);
    lv_label_set_text(ui_HomeLabelCurrentHum, bufftext);


  } else {
    lv_label_set_text(ui_HomeLabelCurrentTemp, "--.-");
    lv_label_set_text(ui_HomeLabelCurrentHum, "--");
  }

  //Actualisation des consignes
  sprintf(bufftext,"Cible %04.1f°C / %02.0f%%",localData.SetTemp,localData.SetHum);
  lv_label_set_text(ui_HomeLabelConsignes, bufftext);

  
  //actualisation des états des actionneurs
  set_switch_state(ui_HomeSwitchHeatState,localData.HeatState);
  set_switch_state(ui_HomeSwitchColdState,localData.ColdState);

  set_switch_state(ui_HomeSwitchDehumState,localData.DehumState);
  set_switch_state(ui_HomeSwitchHumState,localData.HumState);

  set_switch_state(ui_HomeSwitchFanState, localData.FanState);

}



// Gestion du geste vers le haut sur le screen Home
void ScreenHomeGestUp(lv_event_t *e) {
  _ui_screen_change(
      &ui_ScreenTemp,
      LV_SCR_LOAD_ANIM_FADE_ON,
      300,
      0,
      &ui_ScreenTemp_screen_init
  );    
}
    