#include "Wifi/my_WebServer_.h"

#include <Arduino.h>

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "Global/Global.h"

#include "Wifi/PageWeb/web_assets.h"

#include "Wifi/my_wifi.h"


const char*  DefaultPage = "/Wifi.html" ;



void handleRestart(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Restarting...");
  delay(500);
  ESP.restart();
}


wifi_mode_t modeAvantScan = WIFI_OFF;
void handleScanWifi(AsyncWebServerRequest *request) {

  if (WiFi.scanComplete() == -2) { // Si pas de scan en cours
    // 1. On mémorise le mode AVANT le scan
    modeAvantScan = WiFi.getMode(); 
    // 2. On lance le scan en asynchrone
    WiFi.scanNetworks(true); 
    //Set_WifiScanning(true);
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
  options.reserve(1024); // Réserve 1Ko d'un coup
  for (int i = 0; i < n; i++) {
    options += "<option>" + WiFi.SSID(i) + "</option>";
  }

  Serial.print("Réponse scan: ");
  Serial.println(options);

  // --- RESTAURATION DU MODE ---
  if ((WiFi.getMode() != modeAvantScan)  && (modeAvantScan!= WIFI_OFF)) {
    WiFi.mode(modeAvantScan);
    Serial.printf("Restauration du mode Wi-Fi vers : %d\n", modeAvantScan);
  }

  WiFi.scanDelete(); // Libère la mémoire
  
  //Set_WifiScanning(false);
  
  request->send(200, "text/plain", options);

  }

}
void handleSaveWiFi(AsyncWebServerRequest *request) {
  String Ssid = request->arg("Ssid");
  String Pass = request->arg("Pass");

  Save_WIFI_SsidAndPass(Ssid,Pass);

  request->send(200, "text/plain",
    "Paramètres WiFi enregistrés.\r\n\r\nL'appareil redémarre...\r\nVeuillez vous connecter au nouveau réseau.");

}


void setupWebHandlers() {

  // 1. On arrête le serveur s'il tournait déjà
  server.end(); 
  
  // 2. Cela force la libération de la RAM dès que le fichier est envoyé
  DefaultHeaders::Instance().addHeader("Connection", "close");
  DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache");

  // 3. On vide toutes les routes précédemment enregistrées
  server.reset(); // Attention : reset() supprime TOUT, y compris les routes de AddRoutesToServer

  AddRoutesToServer(server,DefaultPage);

  server.on("/RestartESP", HTTP_GET, handleRestart);

  server.on("/Scan", HTTP_GET, handleScanWifi);
  server.on("/GetScanResults", HTTP_GET, handleScanResultWifi);
  server.on("/SaveWifi", HTTP_POST, handleSaveWiFi);



  server.begin();



  Serial.println("Server started");
}

