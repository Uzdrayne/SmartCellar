#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include "DisplayAndTouch/ConfigDisplay.h"
#include <ui.h>

#include "LVGL/Lvgl_Global.h"
#include "Wifi/my_wifi.h"
#include "Global/Global.h"
#include "Controller/CaveController.h"
#include "SerialLink/SerialLink.h"
#include "WifiAndMqtt/my_MQTT.h"

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("SmartCellar READY!");

  InitMutex();
  InitializeSerialLink();
  LoadWifi(DataWifi);
  LoadMqtt(DataMqtt);
  LoadCaveControllerData(DataController);

  my_TouchScreen_Init();
  tft.setBrightness(10);
  ui_init();
  Initialize_ScreenTemp();
  Initialize_ScreenHum();
  Initialize_ScreenFan();
  tft.setBrightness(ScreenUserBrightness);
  UiReady = true;
  ConnectToWifi();
}

unsigned long lastJson = 0;
unsigned long lastWifiScreenRefresh = 0;
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

  if ((now - lastJson) >= 250) {
    lastJson = now;
    Get_Json_GlobalStatus(JSON_GLOBAL_STATUS, sizeof(JSON_GLOBAL_STATUS));
  }

  if (dnsActive) {
    dns.processNextRequest();
  }

  if (UiReady && now - lastWifiScreenRefresh >= 500 && lv_scr_act() == ui_ScreenWifi) {
    lastWifiScreenRefresh = now;
    Refresh_ScreenWifi();
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
