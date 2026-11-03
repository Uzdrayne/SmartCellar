#ifndef CONTROLLER_DATA_H
#define CONTROLLER_DATA_H


#include <Arduino.h>
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

constexpr unsigned int FanDurationMinMinutes = 1;
constexpr unsigned int FanDurationMaxMinutes = 1440;
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


    //----ETATS RELAIS-------
    bool ColdState; // État du relais froid
    bool HeatState; // État du relais chaud
    bool DehumState; // État du relais déhumidificateur
    bool HumState; // État du relais humidificateur
    bool RelayFeedbackValid;
    bool RelayGpioCold;
    bool RelayGpioHeat;
    bool RelayGpioDehum;
    bool RelayGpioHum;
    bool FanEnabled;
    bool FanState;
    unsigned int FanOnMinutes;
    unsigned int FanOffMinutes;
    bool RelayGpioFan;
    unsigned long FanPhaseElapsedSeconds;
    unsigned long FanPhaseDurationSeconds;

    //----MESSAGES DEBUG-------
    char ColdDebugMsg[200];// Message debug froid
    char HeatDebugMsg[200]; // Message debug chaud
    char DehumDebugMsg[200];// Message debug déhumidificateur
    char HumDebugMsg[200];// Message debug humidificateur

};
extern StrctDataController DataController;

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
    
    String MqttServer ;  // MQTT Server URL
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


//extern String JSON_GLOBAL_STATUS ;

extern char JSON_GLOBAL_STATUS[2000];
void Get_Json_GlobalStatus(char* buffer, size_t maxSize);



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
void Save_SetTemp_and_SetHum();
void Save_Wifi_Parameters();
void Save_Mqtt_Parameters();
void Save_SensorOffset();
void Save_ColdHardware();
void Save_HeatHardware();
void Save_DehumHardware();
void Save_HumHardware();
void Save_FanSettings();




void LoadWifi(StrctDataWifi &data);

void LoadMqtt(StrctDataMqtt &data);

void LoadCaveControllerData(StrctDataController &data);





#endif