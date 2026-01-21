#include "Wifi/my_wifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include <ESPmDNS.h>

#include <ui.h>
#include "Global/Global.h"
#include "LVGL/Lvgl_Global.h"

#include "Global/Global.h"

#include "Wifi/my_WebServer_.h"




String WifiSsid = "";
String WifiPass = "";

String APSsid = "SmartDisplay";
String APPass = "123456789";



//HTTPClient http;
const char* hostIP   = "192.168.0.40"; // IP de l'autre ESP
const uint16_t port  = 80;

void Get_WIFI_SsidAndPass(String &Ssid, String &pass){
    prefs.begin("Wifi", true);
    Ssid = prefs.getString("Ssid", "");
    pass = prefs.getString("Pass", "");
    prefs.end();
}

void Save_WIFI_SsidAndPass(const String &Ssid, const String &pass){
    prefs.begin("Wifi", false);
    prefs.putString("Ssid", Ssid);
    prefs.putString("Pass", pass);
    prefs.end();
}

//Evenement Wifi
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    Serial.printf("%d : ",millis());
    Serial.print("Événement WiFi : ");
    
    switch(event) {
        case ARDUINO_EVENT_WIFI_READY:
            Serial.println("WiFi ready");
            break;

        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("STA démarrée");
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("STA connectée à AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("STA déconnectée, raison: %d\n", info.wifi_sta_disconnected.reason);

            WifiSTA_OK=false;

            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("STA a reçu IP: %s \n", WiFi.localIP().toString().c_str());

            WifiSTA_OK=true;
            Gestion_DNS_mDNS();
            LastReadJsonOK = 0;
            InitWebSocket();
            
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("AP démarré");

            WifiSTA_OK=false;
            Gestion_DNS_mDNS();
            setupWebHandlers();
            
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.printf("Client connecté à AP, AID=%d\n", info.wifi_ap_staconnected.aid);
            break;

        default:
            Serial.printf("Autre événement: %d\n", event);
            break;
    }
}

//fonction pour se connecter au wifi (STA)
void ConnectToWifi(){

    TryingWifi = true; //indique qu'on est entrain d'essayer de se connecter

    WifiSTA_OK = false;
    IsWsConnected = false;
    LinkStatus = false;
 
    Get_WIFI_SsidAndPass(WifiSsid,WifiPass);

    WiFi.onEvent(WiFiEvent);

    delay(100);
    WiFi.disconnect();
    delay(500);
    
    if(WifiSsid !=""){

        WiFi.disconnect();
        delay(500);
        WiFi.mode(WIFI_STA); 
        delay(100);
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true); 
        WiFi.setTxPower(WIFI_POWER_8_5dBm);

        Serial.printf("Tentative connexion au WiFi : -%s- -> mdp : -%s-\n", WifiSsid.c_str(), WifiPass.c_str());

        WiFi.begin(WifiSsid, WifiPass);

        WifiAttemptStart = millis();
    }
 
}


//ajoute DNS si mode STA (et DNS end en AP)
//mDNS pour STA et AP
void Gestion_DNS_mDNS(){


    MDNS.end();
    delay(50);

    if(WiFi.status() == WL_CONNECTED){ //STA 
        dns.stop();
        dnsActive = false;
    }
    else {//AP
        dns.setErrorReplyCode(DNSReplyCode::NoError);
        dns.start(53, "*", WiFi.softAPIP()); 
        dnsActive = true;
        Serial.printf("%d : ",millis());
        Serial.println("DNS Start");
    }

    if (!MDNS.begin("esp_client")) {
        Serial.println("Erreur mDNS");
        return;
    }
    MDNS.addService("http", "tcp", 80);
    Serial.printf("%d : ",millis());
    Serial.println("mDNS Start");
  
    

}

//evenement WebSocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length){
    switch(type) {

        case WStype_CONNECTED:
            IsWsConnected = true; // Le lien est établi, mais on n'a pas encore de data
            Serial.printf("%d : ",millis());
            Serial.printf("[WS] Connecté à l'URL : %s\n", payload);
            break;

        case WStype_DISCONNECTED:
            IsWsConnected = false; // Le lien est physiquement coupé
            Serial.printf("%d : ",millis());
            Serial.printf("[WS] Déconnecté ! (Latence possible pendant tentative de reco)\n");
            break;

        case WStype_TEXT:
            //Serial.printf("%d : ",millis());
            //Serial.printf("[WS] Message reçu (%u octets)\n", length);
            ReadJSON(payload, length);

            LastReadJsonOK = millis();  // preuve de vie
            LinkStatus = true;          // lien actif confirmé
            LinkHeart = !LinkHeart;

            break;

        case WStype_ERROR:
            Serial.printf("%d : ",millis());
            Serial.printf("[WS] Erreur détectée\n");
            break;
            
        default:
            break;
    }
}


//init
void InitWebSocket(){

    Serial.printf("%d : ",millis());
    Serial.println("Init WebSocket");

    webSocket.disconnect();

    delay(500);
        
    webSocket.begin(hostIP, 80, "/ws");
    webSocket.onEvent(webSocketEvent);
  
}


//check si on a eu un JSON dans le delay DelayLastJSON, sinon dit qu'on a perdu la com et relance un init 15 sec après

void Check_LinkStatus(const unsigned long now) {

    // 1) Si jamais reçu de JSON, le lien n’est pas "OK"
    if (LastReadJsonOK == 0) {
        LinkStatus = false;
    } else {
        // 2) Sinon, "OK" si on est connecté ET si le dernier JSON est récent
        LinkStatus = (IsWsConnected && (now - LastReadJsonOK <= DelayLastJSON));
    }

    // 3) Si plus de 15s sans JSON, tenter une reconnexion WebSocket (si WiFi OK)
    if (WiFi.status() == WL_CONNECTED) {
        const bool timeoutNoMsg = (LastReadJsonOK == 0) || (now - LastReadJsonOK > WsTimeoutNoMsg);
        const bool cooldownOver = (now - lastWsReconnectAttempt) > WsReconnectCooldown;

        if (timeoutNoMsg && cooldownOver) {
            Serial.printf("%lu : [WS] Timeout prolongé : Tentative de reconnexion...\n", now);
            lastWsReconnectAttempt = now;

            IsWsConnected = false;
            LinkStatus    = false;

            webSocket.disconnect(); // propre
            delay(100);
            InitWebSocket();
        }
    } else {
        // Si pas de WiFi, on évite toute tentative WS
        IsWsConnected = false;
        LinkStatus    = false;
    }
}
