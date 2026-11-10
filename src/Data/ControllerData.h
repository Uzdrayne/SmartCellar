#ifndef CONTROLLER_DATA_H
#define CONTROLLER_DATA_H


#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HomeAssistantMQTT.h>

#include <DNSServer.h>

//#include "WifiAndMqtt/HomeAssistantMQTT.h"


// Stockage persistant (WiFi, capteur, consignes)
extern Preferences prefs;

extern SemaphoreHandle_t DataMutex;

// Déclaration du serveur async
extern AsyncWebServer server;
extern AsyncWebSocket ws;

extern DNSServer dns;


extern WiFiClient wifiClient;
extern HomeAssistantMQTT mqtt;

constexpr unsigned int FanDefaultOnMinutes = 5;
constexpr unsigned int FanDefaultOffMinutes = 55;

struct StrctDataController {

    float Temperature;// Température actuelle mesurée
    float Humidity;// Humidité actuelle mesurée

    float SetTemp;// Consigne température
    float SetHum;// Consigne humidité

    bool TempOffsetEnabled;//bool pour activer ou on l'offset de température    
    float TempOffset;// valeur de l'offset de température

    bool HumOffsetEnabled;//bool pour activer ou on l'offset d'humidité
    float HumOffset;// valeur de l'offset d'humidité


    //------FROID------
    float ColdHysHigh; // °C Delta par rapport consigne pour activer froid
    float ColdHysLow; // °C Delta par rapport consigne pour désactiver froid
    unsigned int  ColdDelayMinOn; // Temps minimum allumé (s)
    unsigned int   ColdDelayMinOff ;  // Temps minimum éteint (s)

    //------CHAUD------
    float HeatHysHigh; // °C Delta par rapport consigne pour desactiver chaud
    float HeatHysLow; // °C Delta par rapport consigne pour activer chaud
    unsigned int  HeatDelayMinOn; // Temps minimum allumé (s)
    unsigned int   HeatDelayMinOff ;  // Temps minimum éteint (s)

    //------DEHUM------
    float DehumHysHigh; // % Delta par rapport consigne pour activer déhumidificateur
    float DehumHysLow; // % Delta par rapport consigne pour désactiver déhumidificateur
    unsigned int  DehumDelayMinOn; // Temps minimum allumé (s)
    unsigned int   DehumDelayMinOff ;  // Temps minimum éteint (s)

    //------CHAUD------
    float HumHysHigh; // % Delta par rapport consigne pour desactiver Humidificateur
    float HumHysLow; // % Delta par rapport consigne pour activer Humidificateur
    unsigned int  HumDelayMinOn; // Temps minimum allumé (s)
    unsigned int   HumDelayMinOff ;  // Temps minimum éteint (s)

    //-----VENTILATION------
    bool FanEnabled;
    unsigned int FanOnMinutes;
    unsigned int FanOffMinutes;
    unsigned long FanPhaseElapsedSeconds;
    unsigned long FanPhaseDurationSeconds;


    //----ETATS RELAIS-------
    bool ColdState; // État du relais froid
    bool HeatState; // État du relais chaud
    bool DehumState; // État du relais déhumidificateur
    bool HumState; // État du relais humidificateur
    bool FanState; // État du relais ventilateur

    //----FEEDBACK GPIO RELAIS-------
    bool RelayFeedbackValid;
    bool RelayGpioCold;
    bool RelayGpioHeat;
    bool RelayGpioDehum;
    bool RelayGpioHum;
    bool RelayGpioFan;

};
extern StrctDataController DataController;

// Explications de régulation affichées par la page Web. Séparées de
// StrctDataController pour ne pas recopier 800 octets à chaque instantané.
constexpr size_t ControllerDebugMessageSize = 200;
struct ControllerDebugMessages {
    char Cold[ControllerDebugMessageSize];
    char Heat[ControllerDebugMessageSize];
    char Dehum[ControllerDebugMessageSize];
    char Hum[ControllerDebugMessageSize];
};
// Protégé par DataMutex.
extern ControllerDebugMessages DataDebugMessages;

struct StrctDataWifi {
    String WifiSsid;
    String WifiPass;
    bool WifiConnected;
    bool WifiIsSTA;
    bool WifiScanning;
};
extern StrctDataWifi DataWifi;

struct StrctDataMqtt {
    bool MqttEnable;
    
    String MqttServer ;  // MQTT broker host name or IP address
    uint16_t MqttPort ;               // MQTT Server port

    String MqttUser ;                    // MQTT Username
    String MqttPassword ;                // MQTT Password

    String MqttManu ;                   // Manufacturer name
    String MqttModel ;             // Device model name
    String MqttVersion ;               // Device firmware version
    String MqttDeviceName ;               // Device name in Home Assistant

    bool MqttConnected;
};
extern StrctDataMqtt DataMqtt;


// Remplit l'état temps réel envoyé aux clients WebSocket (sans les limites).
void FillGlobalStatus(JsonObject status);
// Limites de configuration, constantes : envoyées une seule fois par connexion.
void FillConfigurationLimits(JsonObject limits);



void InitMutex();

void Set_WifiIsSTA(bool val);
bool Get_WifiIsSTA();

void Set_WifiScanning(bool val);
bool Get_WifiScanning();



//MQTT
// Bool : activer ou non MQTT
void Set_MqttEnable(bool val);
bool Get_MqttEnable();
// Bool : MQTT connecté
void Set_MqttConnected(bool val);
bool Get_MqttConnected();


//sauvegarder dans preferences
bool Save_SetTemp();
bool Save_SetHum();
bool Save_Wifi_Parameters();
bool Save_Mqtt_Parameters();
bool Save_SensorOffset();
bool Save_ColdHardware();
bool Save_HeatHardware();
bool Save_DehumHardware();
bool Save_HumHardware();
bool Save_FanSettings();




void LoadWifi(StrctDataWifi &data);

void LoadMqtt(StrctDataMqtt &data);

void LoadCaveControllerData(StrctDataController &data);





#endif