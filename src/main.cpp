#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include "DisplayAndTouch/ConfigDisplay.h"
#include <ui.h>

#include "LVGL/Lvgl_Global.h"
#include "Wifi/my_wifi.h"
#include "Global/Global.h"
#include "Controller/CaveController.h"
#include "Alarm/AlertLog.h"
#include "Alarm/AlertManager.h"
#include "Alarm/SafetyAlarm.h"
#include "ServerWeb/WebSocketApi.h"
#include "SerialLink/SerialLink.h"
#include "ServerWeb/WebSecurity.h"

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("SmartCellar READY!");

  InitMutex();
  InitializeAlertLog();
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

unsigned long lastPeriodicUpdate  = 0;
unsigned long lastMemoryReport = 0;
uint32_t lvglPeakUsed = 0;

void ReportMemoryUsage(unsigned long now) {
  lv_mem_monitor_t lvglMemory;
  lv_mem_monitor(&lvglMemory);
  const uint32_t lvglUsed = lvglMemory.total_size - lvglMemory.free_size;
  if (lvglUsed > lvglPeakUsed) {
    lvglPeakUsed = lvglUsed;
  }

  if (now - lastMemoryReport < 10000) {
    return;
  }
  lastMemoryReport = now;

  Serial.printf(
      "[MEM] heap libre %u, min %u, bloc max %u | LVGL utilise %u/%u (pic %u, frag %u%%) | WS %u\n",
      static_cast<unsigned int>(ESP.getFreeHeap()),
      static_cast<unsigned int>(ESP.getMinFreeHeap()),
      static_cast<unsigned int>(ESP.getMaxAllocHeap()),
      static_cast<unsigned int>(lvglUsed),
      static_cast<unsigned int>(lvglMemory.total_size),
      static_cast<unsigned int>(lvglPeakUsed),
      static_cast<unsigned int>(lvglMemory.frag_pct),
      static_cast<unsigned int>(ws.count()));
}


void loop() {
  unsigned long now = millis();



  ProcessSerialLink(now);
  

  if ((now - lastPeriodicUpdate ) >= 250) {

    UpdateController();
    UpdateSafetyAlarm(now);
    UpdateAlertManager(now);

    lastPeriodicUpdate  = now;
    ProcessWebSocketApi();


    // L'état JSON n'est construit que si un client WebSocket est connecté.
    const bool networkAvailable = WifiSTA_OK || (WiFi.getMode() & WIFI_AP);
    if (networkAvailable && !Get_WifiScanning()) {
      SendValueWebSocket();
    }

    ws.cleanupClients();

    Handle_Screen_Brightness(now);
    ReportMemoryUsage(now);

  }

  
  if (dnsActive) {
    dns.processNextRequest();
  }

  lv_timer_handler();

  delay(1);
}
