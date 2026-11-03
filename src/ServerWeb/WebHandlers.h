#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <ESPAsyncWebServer.h>






/******************************************************
 * Fonction centrale pour configurer les routes
 ******************************************************/
void setupWebHandlers();

/******************************************************
 * HANDLERS PAGES PRINCIPALES
 ******************************************************/
void handleMenu(AsyncWebServerRequest *request);
void handleWiFi(AsyncWebServerRequest *request);
void handleMqtt(AsyncWebServerRequest *request);
void handleCapteur(AsyncWebServerRequest *request);
void handleCold(AsyncWebServerRequest *request);
void handleHeat(AsyncWebServerRequest *request);
void handleDehum(AsyncWebServerRequest *request);
void handleHum(AsyncWebServerRequest *request);

/******************************************************
 * HANDLERS PAGE MENU
 ******************************************************/
void handleGlobalStatus(AsyncWebServerRequest *request);
void handleSaveConsignes(AsyncWebServerRequest *request);

/******************************************************
 * HANDLERS PAGE WIFI
 ******************************************************/
void handleScanWifi(AsyncWebServerRequest *request);
void handleScanResultWifi(AsyncWebServerRequest *request);
void handleSaveWiFi(AsyncWebServerRequest *request);

/******************************************************
 * HANDLERS PAGE MQTT
 ******************************************************/
void handleSaveMqtt(AsyncWebServerRequest *request);

/******************************************************
 * HANDLERS PAGE CAPTEUR
 ******************************************************/
void handleSaveCapteurCalibration(AsyncWebServerRequest *request);

/******************************************************
 * HANDLERS PAGES HARDWARE
 ******************************************************/
void handleColdSave(AsyncWebServerRequest *request);
void handleHeatSave(AsyncWebServerRequest *request);
void handleDehumSave(AsyncWebServerRequest *request);
void handleHumSave(AsyncWebServerRequest *request);

#endif