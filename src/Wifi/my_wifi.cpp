#include "Wifi/my_wifi.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "Data/ControllerData.h"
#include "Global/Global.h"
#include "ServerWeb/WebHandlers.h"
#include "ServerWeb/WebSecurity.h"
#include "WifiAndMqtt/my_MQTT.h"

String APSsid = "SmartCellar";
String APPass;

namespace {

bool webServicesStarted = false;
bool wifiEventRegistered = false;
unsigned long lastWebSocketSend = 0;

void startNetworkServices() {
    if (!webServicesStarted) {
        Set_WebSocket();
        setupWebHandlers();
        webServicesStarted = true;
    }

    Gestion_DNS_mDNS();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        const IPAddress ip = client->remoteIP();
        Serial.printf("[WS] Client %u connecté depuis %s\n", client->id(), ip.toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client %u déconnecté\n", client->id());
    }
}

void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Set_WifiIsSTA(true);
            WifiSTA_OK = false;
            Set_MqttConnected(false);
            xSemaphoreTake(DataMutex, portMAX_DELAY);
            DataWifi.WifiConnected = false;
            xSemaphoreGive(DataMutex);
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
                Serial.printf("Wi-Fi STA déconnecté, raison : %d\n", info.wifi_sta_disconnected.reason);
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Set_WifiIsSTA(true);
            WifiSTA_OK = false;
            TryingWifi = false;
            xSemaphoreTake(DataMutex, portMAX_DELAY);
            DataWifi.WifiConnected = true;
            xSemaphoreGive(DataMutex);
            Serial.printf("Wi-Fi connecté : %s\n", WiFi.localIP().toString().c_str());
            configTzTime("CET-1CEST,M3.5.0,M10.5.0/3",
                         "pool.ntp.org",
                         "time.cloudflare.com",
                         "time.google.com");
            Serial.println("[NTP] Synchronisation horaire demandée");
            startNetworkServices();
            if (Get_MqttEnable()) {
                ConnectMqtt();
            }
            WifiSTA_OK = true;
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            Set_WifiIsSTA(false);
            WifiSTA_OK = false;
            TryingWifi = false;
            Set_MqttConnected(false);
            xSemaphoreTake(DataMutex, portMAX_DELAY);
            DataWifi.WifiConnected = true;
            xSemaphoreGive(DataMutex);
            Serial.printf("Point d'accès actif : %s, IP : %s\n",
                          APSsid.c_str(), WiFi.softAPIP().toString().c_str());
            startNetworkServices();
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP:
            Set_MqttConnected(false);
            xSemaphoreTake(DataMutex, portMAX_DELAY);
            DataWifi.WifiConnected = false;
            xSemaphoreGive(DataMutex);
            break;

        default:
            break;
    }
}

}

void ConnectToWifi() {
    String ssid;
    String password;

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    ssid = DataWifi.WifiSsid;
    password = DataWifi.WifiPass;
    xSemaphoreGive(DataMutex);

    if (!wifiEventRegistered) {
        WiFi.onEvent(onWiFiEvent);
        wifiEventRegistered = true;
    }

    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    if (ssid.isEmpty()) {
        StartAccessPoint();
        return;
    }

    TryingWifi = true;
    WifiSTA_OK = false;
    WifiAttemptStart = millis();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    Serial.printf("Connexion Wi-Fi à %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
}

void StartAccessPoint() {
    TryingWifi = false;
    WifiSTA_OK = false;
    if (APPass.length() < 8) {
        Serial.println("[WiFi] Point d'accès refusé: mot de passe unique indisponible");
        return;
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(APSsid.c_str(), APPass.c_str())) {
        Serial.println("Échec du démarrage du point d'accès Wi-Fi");
    }
}

void Gestion_DNS_mDNS() {
    MDNS.end();

    if (Get_WifiIsSTA()) {
        dns.stop();
        dnsActive = false;
    } else {
        dns.setErrorReplyCode(DNSReplyCode::NoError);
        dns.start(53, "*", WiFi.softAPIP());
        dnsActive = true;
    }

    if (!MDNS.begin("smartcellar")) {
        Serial.println("Échec du démarrage mDNS");
        return;
    }

    MDNS.addService("http", "tcp", 80);
}

void Set_WebSocket() {
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
}

void SendValueWebSocket() {
    const unsigned long now = millis();
    if (now - lastWebSocketSend < 1000 || ws.count() == 0) {
        return;
    }

    lastWebSocketSend = now;
    if (xSemaphoreTake(DataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        ws.textAll(JSON_GLOBAL_STATUS);
        xSemaphoreGive(DataMutex);
    }
}
