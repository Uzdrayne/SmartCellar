#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <stdlib.h>

#include "WebHandlers.h"

#include "ServerWeb/PageWeb/web_assets.h"

#include "Data/ControllerData.h"

// ==============================
// HANDLERS DE PAGES HTML
// ==============================





void handleGlobalStatus(AsyncWebServerRequest *request){

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  request->send(200, "application/json", JSON_GLOBAL_STATUS);
  xSemaphoreGive(DataMutex); // ON LIBÈRE TOUT DE SUITE


}

void handleSaveConsignes(AsyncWebServerRequest *request) {
  if(request->hasArg("Temp") && request->hasArg("Hum")) {
    float NewSetTemp = request->arg("Temp").toFloat();
    float NewSetHum  = request->arg("Hum").toFloat();


    float ActualSetTemp = 0;
    float ActualSetHum = 0;
    bool ActuTemp =false;
    bool ActuHum = false;

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    ActualSetTemp = DataController.SetTemp;
    ActualSetHum = DataController.SetHum;
    if(NewSetTemp != ActualSetTemp) {
       DataController.SetTemp=NewSetTemp;
       ActuTemp=true;
    }
    if(NewSetHum != ActualSetHum)  {
      DataController.SetHum=NewSetHum;
      ActuHum=true;
    }    

    xSemaphoreGive(DataMutex);

    if(ActuTemp){Serial.printf("HTML : SetTemp updated to %.2f\n", NewSetTemp); }
    if(ActuHum){ Serial.printf("HTML : SetHum updated to %.2f\n", NewSetHum); }

    if(ActuTemp || ActuHum){ // si une des deux valeur a bougé on save dans prefs
      Save_SetTemp_and_SetHum();  
    }
    

    request->send(200, "text/plain", "consignes sauvegardées");
  } else {
    request->send(400, "text/plain", "Paramètres manquants !");
  }
}

wifi_mode_t modeAvantScan = WIFI_OFF;
void handleScanWifi(AsyncWebServerRequest *request) {

  if (WiFi.scanComplete() == -2) { // Si pas de scan en cours
    // 1. On mémorise le mode AVANT le scan
    modeAvantScan = WiFi.getMode(); 
    // 2. On lance le scan en asynchrone
    WiFi.scanNetworks(true); 
    Set_WifiScanning(true);
  }
  request->send(200, "text/plain", "SCAN_STARTED");

}
void handleScanResultWifi(AsyncWebServerRequest *request) {

  int n = WiFi.scanComplete();

  if (n == -1) { // Toujours en cours
    request->send(200, "text/plain", "WAITING");
  } 
  else if (n == -2) { // Pas encore lancé
    request->send(200, "text/plain", "NOT_STARTED");
  }
  else { // Scan terminé !
  String options = "";
  for (int i = 0; i < n; i++) {
    options += "<option>" + WiFi.SSID(i) + "</option>";
  }

  // --- RESTAURATION DU MODE ---
  if (modeAvantScan != WIFI_OFF) {
    WiFi.mode(modeAvantScan);
    Serial.printf("Restauration du mode Wi-Fi vers : %d\n", modeAvantScan);
  }

  WiFi.scanDelete(); // Libère la mémoire
  Set_WifiScanning(false);
  request->send(200, "text/plain", options);

  }

}

void handleSaveWiFi(AsyncWebServerRequest *request) {
  String Ssid = request->arg("Ssid");
  String Pass = request->arg("Pass");

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  DataWifi.WifiSsid = Ssid;
  DataWifi.WifiPass = Pass;
  xSemaphoreGive(DataMutex);

  Save_Wifi_Parameters();

  request->send(200, "text/plain","Paramètres WiFi enregistrés.\r\n\r\nL'appareil redémarre...\r\nVeuillez vous connecter au nouveau réseau.");
}

void handleSaveMqtt(AsyncWebServerRequest *request) {
  if(request->hasArg("MqttEnable") && request->hasArg("mqtt_server") &&request->hasArg("mqtt_port") && request->hasArg("MQTTUSER") && request->hasArg("MQTTPASSWORD") && request->hasArg("NAME")) {

    bool MqttEnable = request->arg("MqttEnable").toInt() != 0 ;
    String MqttServer = request->arg("mqtt_server");
    uint16_t MqttPort = request->arg("mqtt_port").toInt();
    String MqttUser = request->arg("MQTTUSER");                   
    String MqttPassword = request->arg("MQTTPASSWORD");              
    String MqttDeviceName = request->arg("NAME");        

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataMqtt.MqttEnable =MqttEnable;
    DataMqtt.MqttServer =MqttServer;
    DataMqtt.MqttPort =MqttPort;
    DataMqtt.MqttUser =MqttUser;
    DataMqtt.MqttPassword =MqttPassword;
    DataMqtt.MqttDeviceName =MqttDeviceName;
    xSemaphoreGive(DataMutex);

    Save_Mqtt_Parameters();

    request->send(200, "text/plain","Paramètres MQTT enregistrés.\r\n\r\nL'appareil redémarre...\r\nAttendre 30 secondes avant de se reconnecter.");
  } else {
    request->send(400, "text/plain", "Paramètres manquants !");
  }
}

void handleSaveCapteurCalibration(AsyncWebServerRequest *request) {
  if(request->hasArg("TempEnabled") && request->hasArg("TempValue") && request->hasArg("HumEnabled") && request->hasArg("HumValue")) {

    bool NewTempOffsetEnabled = request->arg("TempEnabled").toInt() != 0 ;
    float NewTempOffset = request->arg("TempValue").toFloat() ;

    bool NewHumOffsetEnabled = request->arg("HumEnabled").toInt() != 0 ;
    float NewHumOffset = request->arg("HumValue").toFloat();

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.TempOffsetEnabled = NewTempOffsetEnabled ;
    DataController.TempOffset = NewTempOffset ;
    DataController.HumOffsetEnabled = NewHumOffsetEnabled ;
    DataController.HumOffset = NewHumOffset ;

    xSemaphoreGive(DataMutex);

    Save_SensorOffset();

    request->send(200, "text/plain", "Calibration enregistrée !");
  } else request->send(400, "text/plain", "Paramètres manquants !");
}

void handleColdSave(AsyncWebServerRequest *request) {
  if(request->hasArg("HysHigh") && request->hasArg("HysLow") && request->hasArg("DelayMinOn") && request->hasArg("DelayMinOff")) {

    float NewHysHigh = request->arg("HysHigh").toFloat() ;
    float NewHysLow = request->arg("HysLow").toFloat();
    float NewDelayMinOn = request->arg("DelayMinOn").toInt();
    float NewDelayMinOff = request->arg("DelayMinOff").toInt();

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.ColdHysHigh = NewHysHigh;
    DataController.ColdHysLow = NewHysLow;
    DataController.ColdDelayMinOn = NewDelayMinOn;
    DataController.ColdDelayMinOff = NewDelayMinOff;

    xSemaphoreGive(DataMutex);

    Save_ColdHardware();

    request->send(200, "text/plain", "Paramètres Froid enregistrée !");
  } else request->send(400, "text/plain", "Paramètres manquants !");
}

void handleHeatSave(AsyncWebServerRequest *request) {
  if(request->hasArg("HysHigh") && request->hasArg("HysLow") && request->hasArg("DelayMinOn") && request->hasArg("DelayMinOff")) {

    float NewHysHigh = request->arg("HysHigh").toFloat() ;
    float NewHysLow = request->arg("HysLow").toFloat();
    float NewDelayMinOn = request->arg("DelayMinOn").toInt();
    float NewDelayMinOff = request->arg("DelayMinOff").toInt();

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.HeatHysHigh = NewHysHigh;
    DataController.HeatHysLow = NewHysLow;
    DataController.HeatDelayMinOn = NewDelayMinOn;
    DataController.HeatDelayMinOff = NewDelayMinOff;

    xSemaphoreGive(DataMutex);

    Save_HeatHardware();

    request->send(200, "text/plain", "Paramètres Chaud enregistrée !");
  } else request->send(400, "text/plain", "Paramètres manquants !");
}

void handleDehumSave(AsyncWebServerRequest *request) {
  if(request->hasArg("HysHigh") && request->hasArg("HysLow") && request->hasArg("DelayMinOn") && request->hasArg("DelayMinOff")) {

    float NewHysHigh = request->arg("HysHigh").toFloat() ;
    float NewHysLow = request->arg("HysLow").toFloat();
    float NewDelayMinOn = request->arg("DelayMinOn").toInt();
    float NewDelayMinOff = request->arg("DelayMinOff").toInt();

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.DehumHysHigh = NewHysHigh;
    DataController.DehumHysLow = NewHysLow;
    DataController.DehumDelayMinOn = NewDelayMinOn;
    DataController.DehumDelayMinOff = NewDelayMinOff;

    xSemaphoreGive(DataMutex);

    Save_DehumHardware();

    request->send(200, "text/plain", "Paramètres Dehum enregistrée !");
  } else request->send(400, "text/plain", "Paramètres manquants !");
}

void handleHumSave(AsyncWebServerRequest *request) {
  if(request->hasArg("HysHigh") && request->hasArg("HysLow") && request->hasArg("DelayMinOn") && request->hasArg("DelayMinOff")) {

    float NewHysHigh = request->arg("HysHigh").toFloat() ;
    float NewHysLow = request->arg("HysLow").toFloat();
    float NewDelayMinOn = request->arg("DelayMinOn").toInt();
    float NewDelayMinOff = request->arg("DelayMinOff").toInt();

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.HumHysHigh = NewHysHigh;
    DataController.HumHysLow = NewHysLow;
    DataController.HumDelayMinOn = NewDelayMinOn;
    DataController.HumDelayMinOff = NewDelayMinOff;

    xSemaphoreGive(DataMutex);

    Save_HumHardware();

    request->send(200, "text/plain", "Paramètres enregistrée !");
  } else request->send(400, "text/plain", "Paramètres manquants !");
}

bool parseFanDuration(const String& text, unsigned int& duration) {
  const char *start = text.c_str();
  char *end = nullptr;
  const long parsed = strtol(start, &end, 10);
  if (start == end || *end != '\0' ||
      parsed < FanDurationMinMinutes || parsed > FanDurationMaxMinutes) {
    return false;
  }

  duration = static_cast<unsigned int>(parsed);
  return true;
}

void handleFanSave(AsyncWebServerRequest *request) {
  if (!request->hasArg("FanEnabled") ||
      !request->hasArg("FanOnMinutes") ||
      !request->hasArg("FanOffMinutes")) {
    request->send(400, "text/plain", "Paramètres de ventilation manquants.");
    return;
  }

  const String enabledText = request->arg("FanEnabled");
  if (enabledText != "true" && enabledText != "false") {
    request->send(400, "text/plain", "État de ventilation invalide.");
    return;
  }

  unsigned int onMinutes;
  unsigned int offMinutes;
  if (!parseFanDuration(request->arg("FanOnMinutes"), onMinutes) ||
      !parseFanDuration(request->arg("FanOffMinutes"), offMinutes)) {
    request->send(400, "text/plain", "Les durées doivent être comprises entre 1 et 1440 minutes.");
    return;
  }

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  DataController.FanEnabled = enabledText == "true";
  DataController.FanOnMinutes = onMinutes;
  DataController.FanOffMinutes = offMinutes;
  xSemaphoreGive(DataMutex);

  Save_FanSettings();
  request->send(200, "text/plain", "Paramètres de ventilation enregistrés.");
}




void handleRestart(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Restarting...");
  ESP.restart();
}


// ==============================
// SETUP DES ROUTES WEB
// ==============================

const char*  DefaultPage = "/Menu.html" ;

void setupWebHandlers() {

  AddRoutesToServer(server, "/Wifi.html", DefaultPage);

   server.on("/RestartESP", HTTP_GET, handleRestart);

  server.on("/GlobalStatus", HTTP_GET, handleGlobalStatus);

  server.on("/SaveConsignes", HTTP_POST, handleSaveConsignes); 
  
  server.on("/Scan", HTTP_GET, handleScanWifi);
  server.on("/GetScanResults", HTTP_GET, handleScanResultWifi);
  server.on("/SaveWifi", HTTP_POST, handleSaveWiFi);
  server.on("/SaveMqtt", HTTP_POST, handleSaveMqtt);

  server.on("/SaveCapteurCalibration", HTTP_POST, handleSaveCapteurCalibration);

  server.on("/SaveColdParameters", HTTP_POST, handleColdSave);
  server.on("/SaveHeatParameters", HTTP_POST, handleHeatSave);
  server.on("/SaveDehumParameters", HTTP_POST, handleDehumSave);
  server.on("/SaveHumParameters", HTTP_POST, handleHumSave);
  server.on("/SaveFanParameters", HTTP_POST, handleFanSave);


  server.begin();
}