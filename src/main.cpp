#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include "DisplayAndTouch/ConfigDisplay.h"
#include <ui.h>

#include "LVGL/Lvgl_Global.h"
#include "Wifi/my_wifi.h"
#include "Global/Global.h"
#include "Controller/CaveController.h"
#include "Alarm/SafetyAlarm.h"
#include "SerialLink/SerialLink.h"
#include "ServerWeb/WebSecurity.h"
#include "WifiAndMqtt/my_MQTT.h"

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("SmartCellar READY!");

  InitMutex();
  if (!InitializeWifiIdentity()) {
    Serial.println("[WiFi] Le nom générique sera utilisé pour le SSID et mDNS.");
  }
  InitializeSerialLink();
  if (!InitializeSafetyAlarm()) {
    Serial.println("Configuration d'alerte indisponible; seuil par défaut utilisé en mémoire.");
  }
  if (!InitializeWebSecurity(APPass)) {
    Serial.println("Paramètres de sécurité indisponibles; le point d'accès ne sera pas démarré.");
  }

  LoadWifi(DataWifi);

  LoadMqtt(DataMqtt);
  
  LoadCaveControllerData(DataController);

  my_TouchScreen_Init();
  tft.setBrightness(10);
  Initialize_LvglUI();
  
  tft.setBrightness(ScreenUserBrightness);
  UiReady = true;
  ConnectToWifi();
}

unsigned long lastJson = 0;


void loop() {
  unsigned long now = millis();

  if (TryingWifi) {
    if (WifiSTA_OK) {
      TryingWifi = false;
    } else if (now - WifiAttemptStart > 60000) {
      Serial.println("Timeout WiFi");
      TryingWifi = false;
      StartAccessPoint();
    }
  }

  ProcessSerialLink(now);
  
  UpdateController();
  UpdateSafetyAlarm(now);

  if ((now - lastJson) >= 250) {
    lastJson = now;
    Get_Json_GlobalStatus(JSON_GLOBAL_STATUS, sizeof(JSON_GLOBAL_STATUS));
  }

  if (dnsActive) {
    dns.processNextRequest();
  }



  const bool networkAvailable = WifiSTA_OK || (WiFi.getMode() & WIFI_AP);
  if (networkAvailable && !Get_WifiScanning()) {
    SendValueWebSocket();
    if (WifiSTA_OK && Get_MqttEnable()) {
      MyMqttLoop();
    }
  }

  ws.cleanupClients();

  lv_timer_handler();

  Handle_Screen_Brightness(millis());

  delay(1);
}
