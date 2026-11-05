#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <stdlib.h>

#include "WebHandlers.h"

#include "ServerWeb/PageWeb/web_assets.h"

#include "Controller/ConfigurationLimits.h"
#include "Data/ControllerData.h"
#include "Controller/ControllerValidation.h"
#include "Alarm/SafetyAlarm.h"
#include "ServerWeb/WebSecurity.h"

// ==============================
// HANDLERS DE PAGES HTML
// ==============================





void handleGlobalStatus(AsyncWebServerRequest *request){

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", JSON_GLOBAL_STATUS);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
  xSemaphoreGive(DataMutex); // ON LIBÈRE TOUT DE SUITE


}

namespace {

enum class RelaySettingsKind {
  Cold,
  Heat,
  Dehumidification,
  Humidification
};

bool parseRelaySettings(AsyncWebServerRequest *request,
                        float& hysHigh, float& hysLow,
                        unsigned int& delayMinOn, unsigned int& delayMinOff) {
  return request->hasArg("HysHigh") &&
      request->hasArg("HysLow") &&
      request->hasArg("DelayMinOn") &&
      request->hasArg("DelayMinOff") &&
      ParseFiniteFloat(request->arg("HysHigh"), hysHigh) &&
      ParseFiniteFloat(request->arg("HysLow"), hysLow) &&
      ParseBoundedUnsigned(request->arg("DelayMinOn"),
                           MinRelayDelaySeconds, MaxRelayDelaySeconds, delayMinOn) &&
      ParseBoundedUnsigned(request->arg("DelayMinOff"),
                           MinRelayDelaySeconds, MaxRelayDelaySeconds, delayMinOff);
}


void handleRelaySettingsSave(AsyncWebServerRequest *request, RelaySettingsKind kind) {
  float hysHigh;
  float hysLow;
  unsigned int delayMinOn;
  unsigned int delayMinOff;
  if (!parseRelaySettings(request, hysHigh, hysLow, delayMinOn, delayMinOff)) {
    String errorMessage = "Paramètres manquants ou invalides. Vérifiez les nombres et les délais (";
    errorMessage += String(MinRelayDelaySeconds) + " à " +
        String(MaxRelayDelaySeconds) + " s).";
    request->send(400, "text/plain", errorMessage);
    return;
  }

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  StrctDataController candidate = DataController;
  switch (kind) {
    case RelaySettingsKind::Cold:
      candidate.ColdHysHigh = hysHigh;
      candidate.ColdHysLow = hysLow;
      candidate.ColdDelayMinOn = delayMinOn;
      candidate.ColdDelayMinOff = delayMinOff;
      break;
    case RelaySettingsKind::Heat:
      candidate.HeatHysHigh = hysHigh;
      candidate.HeatHysLow = hysLow;
      candidate.HeatDelayMinOn = delayMinOn;
      candidate.HeatDelayMinOff = delayMinOff;
      break;
    case RelaySettingsKind::Dehumidification:
      candidate.DehumHysHigh = hysHigh;
      candidate.DehumHysLow = hysLow;
      candidate.DehumDelayMinOn = delayMinOn;
      candidate.DehumDelayMinOff = delayMinOff;
      break;
    case RelaySettingsKind::Humidification:
      candidate.HumHysHigh = hysHigh;
      candidate.HumHysLow = hysLow;
      candidate.HumDelayMinOn = delayMinOn;
      candidate.HumDelayMinOff = delayMinOff;
      break;
  }

  const bool valid = kind == RelaySettingsKind::Cold || kind == RelaySettingsKind::Heat
      ? IsValidTemperatureRelaySettings(candidate)
      : IsValidHumidityRelaySettings(candidate);

  if (valid) {
    switch (kind) {
      case RelaySettingsKind::Cold:
        DataController.ColdHysHigh = hysHigh;
        DataController.ColdHysLow = hysLow;
        DataController.ColdDelayMinOn = delayMinOn;
        DataController.ColdDelayMinOff = delayMinOff;
        break;
      case RelaySettingsKind::Heat:
        DataController.HeatHysHigh = hysHigh;
        DataController.HeatHysLow = hysLow;
        DataController.HeatDelayMinOn = delayMinOn;
        DataController.HeatDelayMinOff = delayMinOff;
        break;
      case RelaySettingsKind::Dehumidification:
        DataController.DehumHysHigh = hysHigh;
        DataController.DehumHysLow = hysLow;
        DataController.DehumDelayMinOn = delayMinOn;
        DataController.DehumDelayMinOff = delayMinOff;
        break;
      case RelaySettingsKind::Humidification:
        DataController.HumHysHigh = hysHigh;
        DataController.HumHysLow = hysLow;
        DataController.HumDelayMinOn = delayMinOn;
        DataController.HumDelayMinOff = delayMinOff;
        break;
    }
  }
  xSemaphoreGive(DataMutex);

  if (!valid) {
    request->send(400, "text/plain",
                  "Hystérésis incohérentes ou hors limites; les valeurs existantes sont conservées.");
    return;
  }

  bool saved = false;
  const char* successMessage = "";
  switch (kind) {
    case RelaySettingsKind::Cold:
      saved = Save_ColdHardware();
      successMessage = "Paramètres froid enregistrés.";
      break;
    case RelaySettingsKind::Heat:
      saved = Save_HeatHardware();
      successMessage = "Paramètres chauffage enregistrés.";
      break;
    case RelaySettingsKind::Dehumidification:
      saved = Save_DehumHardware();
      successMessage = "Paramètres déshumidification enregistrés.";
      break;
    case RelaySettingsKind::Humidification:
      saved = Save_HumHardware();
      successMessage = "Paramètres humidification enregistrés.";
      break;
  }

  if (!saved) {
    request->send(500, "text/plain",
                  "Échec de sauvegarde NVS; les valeurs ont été appliquées en mémoire.");
    return;
  }
  request->send(200, "text/plain", successMessage);
}

}

void handleSaveConsignes(AsyncWebServerRequest *request) {
  float newSetTemp;
  float newSetHum;
  if (!request->hasArg("Temp") || !request->hasArg("Hum") ||
      !ParseFiniteFloat(request->arg("Temp"), newSetTemp) ||
      !ParseFiniteFloat(request->arg("Hum"), newSetHum) ||
      !IsValidSetpoints(newSetTemp, newSetHum)) {
    String errorMessage = "Consignes invalides : température ";
    errorMessage += String(MinTemperatureSetpointC, 2) + "–" +
        String(MaxTemperatureSetpointC, 2) + " °C, humidité " +
        String(MinHumiditySetpointPercent, 2) + "–" +
        String(MaxHumiditySetpointPercent, 2) + " %.";
    request->send(400, "text/plain", errorMessage);
    return;
  }

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  const bool tempChanged = newSetTemp != DataController.SetTemp;
  const bool humChanged = newSetHum != DataController.SetHum;
  DataController.SetTemp = newSetTemp;
  DataController.SetHum = newSetHum;
  xSemaphoreGive(DataMutex);

  if (tempChanged) Serial.printf("HTML : SetTemp updated to %.2f\n", newSetTemp);
  if (humChanged) Serial.printf("HTML : SetHum updated to %.2f\n", newSetHum);
  if ((tempChanged || humChanged) && !Save_SetTemp_and_SetHum()) {
    request->send(500, "text/plain",
                  "Échec de sauvegarde NVS; les consignes ont été appliquées en mémoire.");
    return;
  }

  request->send(200, "text/plain", "Consignes sauvegardées.");
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
  if(request->hasArg("MqttEnable") && request->hasArg("mqtt_server") &&
     request->hasArg("mqtt_port") && request->hasArg("MQTTUSER") &&
     request->hasArg("NAME")) {

    unsigned int mqttPortValue;
    if (!ParseBoundedUnsigned(request->arg("mqtt_port"), MinMqttPort,
                              MaxMqttPort, mqttPortValue)) {
      String errorMessage = "Port MQTT invalide : il doit être compris entre ";
      errorMessage += String(MinMqttPort) + " et " + String(MaxMqttPort) + ".";
      request->send(400, "text/plain", errorMessage);
      return;
    }

    bool MqttEnable = request->arg("MqttEnable").toInt() != 0 ;
    String MqttServer = request->arg("mqtt_server");
    const uint16_t MqttPort = static_cast<uint16_t>(mqttPortValue);
    String MqttUser = request->arg("MQTTUSER");                   
    const String MqttPassword = request->hasArg("MQTTPASSWORD")
        ? request->arg("MQTTPASSWORD")
        : "";
    const String clearPasswordText = request->hasArg("ClearMqttPassword")
        ? request->arg("ClearMqttPassword")
        : "0";
    String MqttDeviceName = request->arg("NAME");        

    if ((clearPasswordText != "0" && clearPasswordText != "1") ||
        (clearPasswordText == "1" && !MqttPassword.isEmpty())) {
      request->send(400, "text/plain", "Action sur le mot de passe MQTT invalide.");
      return;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataMqtt.MqttEnable =MqttEnable;
    DataMqtt.MqttServer =MqttServer;
    DataMqtt.MqttPort =MqttPort;
    DataMqtt.MqttUser =MqttUser;
    if (clearPasswordText == "1") {
      DataMqtt.MqttPassword = "";
    } else if (!MqttPassword.isEmpty()) {
      DataMqtt.MqttPassword = MqttPassword;
    }
    DataMqtt.MqttDeviceName =MqttDeviceName;
    xSemaphoreGive(DataMutex);

    Save_Mqtt_Parameters();

    request->send(200, "text/plain","Paramètres MQTT enregistrés.\r\n\r\nL'appareil redémarre...\r\nAttendre 30 secondes avant de se reconnecter.");
  } else {
    request->send(400, "text/plain", "Paramètres manquants !");
  }
}

void handleConfigureAdmin(AsyncWebServerRequest *request) {
  if (HasWebAdminCredential()) {
    request->send(409, "text/plain", "Le compte administrateur est déjà configuré.");
    return;
  }

  if (!request->hasArg("password") || !request->hasArg("password_confirm")) {
    request->send(400, "text/plain", "Mot de passe manquant.");
    return;
  }

  const String password = request->arg("password");
  if (password.length() < 12 || password != request->arg("password_confirm")) {
    request->send(
        400, "text/plain",
        "Les mots de passe doivent correspondre et contenir au moins 12 caractères.");
    return;
  }

  if (!SaveWebAdminCredential(password)) {
    request->send(500, "text/plain", "Impossible d'enregistrer le compte administrateur.");
    return;
  }

  request->send(200, "text/plain", "Compte administrateur créé.");
}

void handleSaveCapteurCalibration(AsyncWebServerRequest *request) {
  bool newTempOffsetEnabled;
  bool newHumOffsetEnabled;
  float newTempOffset;
  float newHumOffset;
  if (!request->hasArg("TempEnabled") || !request->hasArg("TempValue") ||
      !request->hasArg("HumEnabled") || !request->hasArg("HumValue") ||
      !ParseBoolean(request->arg("TempEnabled"), newTempOffsetEnabled) ||
      !ParseFiniteFloat(request->arg("TempValue"), newTempOffset) ||
      !ParseBoolean(request->arg("HumEnabled"), newHumOffsetEnabled) ||
      !ParseFiniteFloat(request->arg("HumValue"), newHumOffset) ||
      !IsValidTemperatureOffset(newTempOffset) ||
      !IsValidHumidityOffset(newHumOffset)) {
    String errorMessage = "Calibration invalide : offset température ";
    errorMessage += String(MinTemperatureOffsetC, 2) + " à " +
        String(MaxTemperatureOffsetC, 2) + " °C, humidité " +
        String(MinHumidityOffsetPercent, 2) + " à " +
        String(MaxHumidityOffsetPercent, 2) + " points.";
    request->send(400, "text/plain", errorMessage);
    return;
  }

  xSemaphoreTake(DataMutex, portMAX_DELAY);
  DataController.TempOffsetEnabled = newTempOffsetEnabled;
  DataController.TempOffset = newTempOffset;
  DataController.HumOffsetEnabled = newHumOffsetEnabled;
  DataController.HumOffset = newHumOffset;
  xSemaphoreGive(DataMutex);

  if (!Save_SensorOffset()) {
    request->send(500, "text/plain",
                  "Échec de sauvegarde NVS; la calibration a été appliquée en mémoire.");
    return;
  }
  request->send(200, "text/plain", "Calibration enregistrée.");
}

void handleColdSave(AsyncWebServerRequest *request) {
  handleRelaySettingsSave(request, RelaySettingsKind::Cold);
}

void handleHeatSave(AsyncWebServerRequest *request) {
  handleRelaySettingsSave(request, RelaySettingsKind::Heat);
}

void handleDehumSave(AsyncWebServerRequest *request) {
  handleRelaySettingsSave(request, RelaySettingsKind::Dehumidification);
}

void handleHumSave(AsyncWebServerRequest *request) {
  handleRelaySettingsSave(request, RelaySettingsKind::Humidification);
}

bool parseFanDuration(const String& text, unsigned int& duration) {
  return ParseBoundedUnsigned(text, FanDurationMinMinutes,
                              FanDurationMaxMinutes, duration);
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
    String errorMessage = "Les durées doivent être comprises entre ";
    errorMessage += String(FanDurationMinMinutes) + " et " +
        String(FanDurationMaxMinutes) + " minutes.";
    request->send(400, "text/plain", errorMessage);
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

void handleSafetyAlarmSettings(AsyncWebServerRequest *request) {
  const SafetyAlarmConfiguration configuration =
      GetSafetyAlarmConfiguration();
  const SafetyAlarmStatus status = GetSafetyAlarmStatus();

  JsonDocument document;
  document["thresholdC"] = configuration.HighTemperatureThresholdC;
  document["thresholdMinC"] = SafetyAlarmThresholdMinC;
  document["thresholdMaxC"] = SafetyAlarmThresholdMaxC;
  document["triggerDelaySeconds"] = SafetyAlarmTriggerDelaySeconds;
  document["recoveryDelaySeconds"] = SafetyAlarmRecoveryDelaySeconds;
  document["alertActive"] = status.AlertActive;
  document["alertReason"] = GetSafetyAlarmReasonText(status);
  if (status.HasLastMeasurement) {
    document["lastValidTemperatureC"] = status.LastValidTemperatureC;
    document["lastMeasurementAgeSeconds"] = status.LastMeasurementAgeSeconds;
  } else {
    document["lastValidTemperatureC"] = nullptr;
    document["lastMeasurementAgeSeconds"] = nullptr;
  }

  String responseBody;
  if (serializeJson(document, responseBody) == 0) {
    request->send(500, "text/plain", "Impossible de préparer l'état des alertes.");
    return;
  }

  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", responseBody);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void handleSafetyAlarmSave(AsyncWebServerRequest *request) {
  if (!request->hasArg("thresholdC")) {
    request->send(400, "text/plain", "Seuil de température manquant.");
    return;
  }

  float thresholdC;
  if (!ParseFiniteFloat(request->arg("thresholdC"), thresholdC) ||
      thresholdC < SafetyAlarmThresholdMinC ||
      thresholdC > SafetyAlarmThresholdMaxC) {
    String errorMessage = "Le seuil doit être compris entre ";
    errorMessage += String(SafetyAlarmThresholdMinC, 2) + " et " +
        String(SafetyAlarmThresholdMaxC, 2) + " °C.";
    request->send(400, "text/plain", errorMessage);
    return;
  }

  if (!SaveSafetyAlarmThreshold(thresholdC)) {
    request->send(500, "text/plain", "Impossible d'enregistrer le seuil d'alerte.");
    return;
  }

  request->send(200, "text/plain", "Seuil d'alerte enregistré.");
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

  server.on("/RestartESP", HTTP_POST, handleRestart);
  server.on("/ConfigureAdmin", HTTP_POST, handleConfigureAdmin);

  server.on("/GlobalStatus", HTTP_GET, handleGlobalStatus);

  server.on("/SaveConsignes", HTTP_POST, handleSaveConsignes); 
  
  server.on("/Scan", HTTP_POST, handleScanWifi);
  server.on("/GetScanResults", HTTP_GET, handleScanResultWifi);
  server.on("/SaveWifi", HTTP_POST, handleSaveWiFi);
  server.on("/SaveMqtt", HTTP_POST, handleSaveMqtt);

  server.on("/SaveCapteurCalibration", HTTP_POST, handleSaveCapteurCalibration);

  server.on("/SaveColdParameters", HTTP_POST, handleColdSave);
  server.on("/SaveHeatParameters", HTTP_POST, handleHeatSave);
  server.on("/SaveDehumParameters", HTTP_POST, handleDehumSave);
  server.on("/SaveHumParameters", HTTP_POST, handleHumSave);
  server.on("/SaveFanParameters", HTTP_POST, handleFanSave);
  server.on("/AlarmSettings", HTTP_GET, handleSafetyAlarmSettings);
  server.on("/SaveAlarmSettings", HTTP_POST, handleSafetyAlarmSave);

  AddWebSecurityMiddleware(server);

  server.begin();
}