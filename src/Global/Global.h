#ifndef MY_GLOBAL_H
#define  MY_GLOBAL_H

#include <Arduino.h>
#include <lvgl.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

extern Preferences prefs;

extern AsyncWebServer server;
extern DNSServer dns;
extern WebSocketsClient webSocket;



// Etat lien & timers
extern bool TryingWifi;
extern unsigned long WifiAttemptStart;
extern bool WifiSTA_OK;
extern bool IsWsConnected;
extern bool LinkStatus;
extern bool LinkHeart; //bool qui change d'état à chaque fois qu'on recoit un message 
extern unsigned long LastReadJsonOK;        // 0 => jamais reçu de JSON
extern const unsigned long DelayLastJSON; // 5 s pour considérer le lien "vivant"
extern const unsigned long WsTimeoutNoMsg; // 15 s sans message => reconnect
extern unsigned long lastWsReconnectAttempt;
extern const unsigned long WsReconnectCooldown; // 5 s entre 2 tentatives
extern bool dnsActive;

//ECRAN
extern unsigned long LastTouch ;
extern u_int8_t ScreenUserBrightness;
extern bool ScreenAutoOff;
extern unsigned long  ScreenAutoOffTime;
extern bool ScreenON;
extern bool UiReady;

extern lv_timer_t * Timer_ScreenRefresh ; 


//Structure DATA
struct StrctDataController {

    float Temperature;// Température actuelle mesurée
    float Humidity;// Humidité actuelle mesurée

    float TemperatureCorr;// Température actuelle avec offset capteur
    float HumidityCorr;// Humidité actuelle corrigé avec offset captuer

    float SetTemp;// Consigne température
    float SetHum;// Consigne humidité

    int SensorType; // Type de capteur connecté : 0 = DS18B20, 1 = DHT11, 2 = DHT12

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

    //----MESSAGES DEBUG-------
    char ColdDebugMsg[200];// Message debug froid
    char HeatDebugMsg[200]; // Message debug chaud
    char DehumDebugMsg[200];// Message debug déhumidificateur
    char HumDebugMsg[200];// Message debug humidificateur

};
extern StrctDataController DataController;


//pour le sprintf
extern char bufftext[];




void InitControllerData(StrctDataController &data);

void ReadJSON(const uint8_t * payload, size_t length) ;

void Handle_Screen_Brightness(const unsigned long now);


#endif