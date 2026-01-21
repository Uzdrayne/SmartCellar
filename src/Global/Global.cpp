#include "Global/Global.h"

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include <WebSocketsClient.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>


#include "DisplayAndTouch/ConfigDisplay.h"

Preferences prefs;


WebSocketsClient webSocket;
DNSServer dns;
AsyncWebServer server(80);

//ECRAN
unsigned long LastTouch = 0; //millis au dernier appuis sur l'écran tactile
u_int8_t ScreenUserBrightness = 255;
bool ScreenAutoOff = true;
unsigned long  ScreenAutoOffTime = 60000;
bool ScreenON = true;
bool UiReady = false;
lv_timer_t * Timer_ScreenRefresh = nullptr;


// Etat lien & timers
bool TryingWifi = false;
unsigned long WifiAttemptStart = 0;
bool WifiSTA_OK = false;
bool IsWsConnected   = false;
bool LinkStatus      = false;
bool LinkHeart ; //bool qui change d'état à chaque fois qu'on recoit un message 
unsigned long LastReadJsonOK = 0;        // 0 => jamais reçu de JSON
const unsigned long DelayLastJSON = 5000; // 5 s pour considérer le lien "vivant"
const unsigned long WsTimeoutNoMsg = 15000; // 15 s sans message => reconnect
unsigned long lastWsReconnectAttempt = 0;
const unsigned long WsReconnectCooldown = 5000; // 5 s entre 2 tentatives
bool dnsActive = false;

//Structure DATA
StrctDataController DataController;



//pour le sprintf
char bufftext[100];



void InitControllerData(StrctDataController &data){
    
    // ================== VALURS C ==================
    data.Temperature=0;
    data.Humidity=0;
    data.TemperatureCorr=0;
    data.HumidityCorr=0;

    // ================== CONSIGNES ==================
    data.SetTemp = 0;
    data.SetHum = 0;

    // ================== CAPTEUR ==================
    data.SensorType = 0; // DS18B20 par défaut

    data.TempOffsetEnabled = 0; //bool pour activer ou on l'offset de température
    data.TempOffset = 0; // valeur de l'offset de température

    data.HumOffsetEnabled =0; //bool pour activer ou on l'offset d'humidité
    data.HumOffset = 0; // valeur de l'offset de l'humidité


    // ================== FROID ==================
    data.ColdHysHigh= 0; // °C Delta par rapport consigne pour activer froid
    data.ColdHysLow= 0; // °C Delta par rapport consigne pour désactiver froid
    data.ColdDelayMinOn= 0; // Temps minimum allumé
    data.ColdDelayMinOff= 0;// Temps minimum éteint 

    // ================== CHAUD ==================
    data.HeatHysHigh = 0; // °C Delta par rapport consigne pour désactiver chaud
    data.HeatHysLow = 0; // °C Delta par rapport consigne pour activer chaud
    data.HeatDelayMinOn = 0;  // Temps minimum allumé (s)
    data.HeatDelayMinOff = 0;  // Temps minimum éteint (s)

    // ================== DEHUM ==================
    data.DehumHysHigh= 0; // % Delta par rapport consigne pour activer déhumidificateur
    data.DehumHysLow= 0; // % Delta par rapport consigne pour désactiver déhumidificateur
    data.DehumDelayMinOn= 0; // Temps minimum allumé
    data.DehumDelayMinOff= 0;// Temps minimum éteint 

    // ================== HUM ==================
    data.HumHysHigh = 0; // % Delta par rapport consigne pour désactiver Humidificateur
    data.HumHysLow = 0; // % Delta par rapport consigne pour activer Humidificateur
    data.HumDelayMinOn = 0;  // Temps minimum allumé (s)
    data.HumDelayMinOff = 0;  // Temps minimum éteint (s)

        //----ETATS RELAIS-------
    bool ColdState=false; // État du relais froid
    bool HeatState=false; // État du relais chaud
    bool DehumState=false; // État du relais déhumidificateur
    bool HumState=false; // État du relais humidificateur

    //----MESSAGES DEBUG-------
    char ColdDebugMsg[200]="";// Message debug froid
    char HeatDebugMsg[200]="";; // Message debug chaud
    char DehumDebugMsg[200]="";;// Message debug déhumidificateur
    char HumDebugMsg[200]="";;// Message debug humidificateur
    
}

void ReadJSON(const uint8_t * payload, size_t length) {

    // Parse JSON
    JsonDocument doc; // Taille à ajuster selon ton JSON
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {

        // ================== VALURS C ==================
        DataController.Temperature = doc["Sensor"]["Temp"] | 0.0f ;
        DataController.Humidity  = doc["Sensor"]["Hum"] | 0.0f ;

        // ================== CONSIGNES ==================
        DataController.SetTemp = doc["Consigne"]["SetTemp"] | 0.0f ;
        DataController.SetHum  = doc["Consigne"]["SetHum"] | 0.0f ;

        // ================== CAPTEUR ==================
        DataController.SensorType       = doc["SensorConfig"]["SensorType"] | 0;
        DataController.TempOffsetEnabled = doc["SensorConfig"]["TempOffsetEnabled"] | false ;
        DataController.TempOffset        = doc["SensorConfig"]["TempOffset"] | 0.0f ;
        DataController.HumOffsetEnabled  = doc["SensorConfig"]["HumOffsetEnabled"] | false ;
        DataController.HumOffset         = doc["SensorConfig"]["HumOffset"] | 0.0f ;

        // ================== FROID ==================
        DataController.ColdHysHigh   = doc["HardwareCold"]["HysHigh"] | 0.0f ;
        DataController.ColdHysLow    = doc["HardwareCold"]["HysLow"] | 0.0f ;
        DataController.ColdDelayMinOn  = doc["HardwareCold"]["DelayMinOn"] | 0;
        DataController.ColdDelayMinOff = doc["HardwareCold"]["DelayMinOff"] | 0;

        // ================== CHAUD ==================
        DataController.HeatHysHigh    = doc["HardwareHeat"]["HysHigh"] | 0.0f ;
        DataController.HeatHysLow     = doc["HardwareHeat"]["HysLow"] | 0.0f ;
        DataController.HeatDelayMinOn  = doc["HardwareHeat"]["DelayMinOn"] | 0;
        DataController.HeatDelayMinOff = doc["HardwareHeat"]["DelayMinOff"] | 0;

        // ================== DEHUM ==================
        DataController.DehumHysHigh    = doc["HardwareDehum"]["HysHigh"] | 0.0f ;
        DataController.DehumHysLow     = doc["HardwareDehum"]["HysLow"] | 0.0f ;
        DataController.DehumDelayMinOn  = doc["HardwareDehum"]["DelayMinOn"] | 0;
        DataController.DehumDelayMinOff = doc["HardwareDehum"]["DelayMinOff"] | 0;

        // ================== HUM ==================
        DataController.HumHysHigh    = doc["HardwareHum"]["HysHigh"] | 0.0f ;
        DataController.HumHysLow     = doc["HardwareHum"]["HysLow"] | 0.0f ;
        DataController.HumDelayMinOn  = doc["HardwareHum"]["DelayMinOn"] | 0;
        DataController.HumDelayMinOff = doc["HardwareHum"]["DelayMinOff"] | 0;

        // ================== ETATS RELAIS ==================
        DataController.ColdState   = doc["State"]["StateCold"] | false ;
        DataController.HeatState   = doc["State"]["StateHeat"] | false ;
        DataController.DehumState  = doc["State"]["StateDehum"] | false ;
        DataController.HumState    = doc["State"]["StateHum"] | false ;

        // ================== MESSAGES DEBUG ==================
        strncpy(DataController.ColdDebugMsg,   doc["DebugMsg"]["ColdDebugMsg"] | "", sizeof(DataController.ColdDebugMsg));
        strncpy(DataController.HeatDebugMsg,   doc["DebugMsg"]["HeatDebugMsg"] | "", sizeof(DataController.HeatDebugMsg));
        strncpy(DataController.DehumDebugMsg,  doc["DebugMsg"]["DehumDebugMsg"] | "", sizeof(DataController.DehumDebugMsg));
        strncpy(DataController.HumDebugMsg,    doc["DebugMsg"]["HumDebugMsg"] | "", sizeof(DataController.HumDebugMsg));

        DataController.TemperatureCorr = DataController.Temperature +  (DataController.TempOffsetEnabled ? DataController.TempOffset : 0.0f);
        DataController.HumidityCorr = DataController.Humidity + (DataController.HumOffsetEnabled ? DataController.HumOffset : 0.0f);

    } else {
        Serial.print("Erreur JSON : ");
        Serial.println(error.c_str());
    }


}

void Handle_Screen_Brightness(const unsigned long now) {
    
    lv_disp_trig_activity(NULL);

    // Gestion de l'extinction (Auto-Off)
    if (ScreenAutoOff && ScreenON && (now - LastTouch >= ScreenAutoOffTime)) {
        ScreenON = false;
        tft.setBrightness(10);
        Serial.println("Mode Sommeil : Luminosité réduite");
    }

    // Gestion du réveil (Auto-On)
    // On détecte si on vient de toucher l'écran (LastTouch est récent)
    if (!ScreenON && (now - LastTouch < 500)) { 
        ScreenON = true;
        tft.setBrightness(ScreenUserBrightness);
        Serial.println("Mode Actif : Luminosité rétablie");
    }
}
