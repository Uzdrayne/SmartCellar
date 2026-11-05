#include <Arduino.h>
#include <Preferences.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include <HomeAssistantMQTT.h>

#include <DNSServer.h>

#include "ControllerData.h"
#include "Controller/ConfigurationLimits.h"
#include "Controller/ControllerValidation.h"


SemaphoreHandle_t DataMutex;


Preferences prefs;

namespace {

void ensureFloatDefault(const char* key, float defaultValue, bool& defaultStored) {
    if (prefs.isKey(key)) {
        return;
    }

    if (prefs.putFloat(key, defaultValue) == sizeof(defaultValue)) {
        defaultStored = true;
    } else {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

void ensureUIntDefault(const char* key, unsigned int defaultValue, bool& defaultStored) {
    if (prefs.isKey(key)) {
        return;
    }

    if (prefs.putUInt(key, defaultValue) == sizeof(defaultValue)) {
        defaultStored = true;
    } else {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

void ensureBoolDefault(const char* key, bool defaultValue, bool& defaultStored) {
    if (prefs.isKey(key)) {
        return;
    }

    if (prefs.putBool(key, defaultValue) == sizeof(defaultValue)) {
        defaultStored = true;
    } else {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

void repairFloat(const char* key, float& value, float defaultValue) {
    value = defaultValue;
    Serial.printf("[NVS] Valeur invalide pour %s; valeur par defaut restauree.\n", key);
    if (prefs.putFloat(key, defaultValue) != sizeof(defaultValue)) {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

void repairUInt(const char* key, unsigned int& value, unsigned int defaultValue) {
    value = defaultValue;
    Serial.printf("[NVS] Valeur invalide pour %s; valeur par defaut restauree.\n", key);
    if (prefs.putUInt(key, defaultValue) != sizeof(defaultValue)) {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

void repairBool(const char* key, bool& value, bool defaultValue) {
    value = defaultValue;
    Serial.printf("[NVS] Valeur invalide pour %s; valeur par defaut restauree.\n", key);
    if (prefs.putBool(key, defaultValue) != sizeof(defaultValue)) {
        Serial.printf("[NVS] Echec d'enregistrement de la valeur par defaut: %s\n", key);
    }
}

}


// Déclaration du serveur async
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

DNSServer dns;

WiFiClient wifiClient;
HomeAssistantMQTT mqtt;


StrctDataController DataController;
StrctDataWifi DataWifi;
StrctDataMqtt DataMqtt;


//String JSON_GLOBAL_STATUS;

char JSON_GLOBAL_STATUS[2700];


void InitMutex(){
 DataMutex = xSemaphoreCreateMutex();
 assert(DataMutex != NULL);
}




void Set_WifiIsSTA(bool val){ xSemaphoreTake(DataMutex, portMAX_DELAY); DataWifi.WifiIsSTA= val; xSemaphoreGive(DataMutex); }
bool Get_WifiIsSTA(){ xSemaphoreTake(DataMutex, portMAX_DELAY); bool v = DataWifi.WifiIsSTA; xSemaphoreGive(DataMutex); return v; }

void Set_WifiScanning(bool val){ xSemaphoreTake(DataMutex, portMAX_DELAY); DataWifi.WifiScanning= val; xSemaphoreGive(DataMutex); }
bool Get_WifiScanning(){ xSemaphoreTake(DataMutex, portMAX_DELAY); bool v = DataWifi.WifiScanning; xSemaphoreGive(DataMutex); return v; }



// Bool : activer ou non MQTT
void Set_MqttEnable(bool val){ xSemaphoreTake(DataMutex, portMAX_DELAY); DataMqtt.MqttEnable = val; xSemaphoreGive(DataMutex); }
bool Get_MqttEnable(){ xSemaphoreTake(DataMutex, portMAX_DELAY); bool v = DataMqtt.MqttEnable; xSemaphoreGive(DataMutex); return v; }

void Set_MqttConnected(bool val){ xSemaphoreTake(DataMutex, portMAX_DELAY); DataMqtt.MqttConnected = val; xSemaphoreGive(DataMutex); }
bool Get_MqttConnected(){ xSemaphoreTake(DataMutex, portMAX_DELAY); bool v = DataMqtt.MqttConnected; xSemaphoreGive(DataMutex); return v; }







void LoadWifi(StrctDataWifi &data){
    prefs.begin("MyWifi",false);
    data.WifiSsid = prefs.getString("Ssid","");
    data.WifiPass = prefs.getString("Pass","");
    prefs.end();

    data.WifiConnected= false;
    data.WifiIsSTA = false;
    data.WifiScanning = false;
}

void LoadMqtt(StrctDataMqtt &data){

    prefs.begin("MyMqtt",false);

    data.MqttEnable = prefs.getBool("MqttEnable", false);

    data.MqttServer = prefs.getString("MqttServer","");// MQTT Server URL
    const unsigned int storedMqttPort =
        prefs.getUInt("MqttPort", DefaultMqttPort);
    if (storedMqttPort < MinMqttPort || storedMqttPort > MaxMqttPort) {
        Serial.printf("[NVS] Port MQTT invalide (%u), réinitialisé à %u.\n",
                      storedMqttPort, DefaultMqttPort);
        data.MqttPort = static_cast<uint16_t>(DefaultMqttPort);
        if (prefs.putUInt("MqttPort", DefaultMqttPort) != sizeof(DefaultMqttPort)) {
            Serial.println("[NVS] Échec d'enregistrement du port MQTT par défaut.");
        }
    } else {
        data.MqttPort = static_cast<uint16_t>(storedMqttPort);
    }

    data.MqttUser = prefs.getString("MqttUser","");// MQTT Username
    data.MqttPassword = prefs.getString("MqttPass","");//MQTT Password
    
    data.MqttDeviceName = prefs.getString("MqttName","SmartCellar");// Device name in Home Assistant

    prefs.end();


    data.MqttManu = "JR" ; // Manufacturer name
    data.MqttModel = "SmartCellar";
    data.MqttVersion = "v01";

    data.MqttConnected = false;

}

void LoadCaveControllerData(StrctDataController &data){

    data.Temperature = NAN;
    data.Humidity = NAN;
    data.ColdState = false;
    data.HeatState = false;
    data.DehumState = false;
    data.HumState = false;
    data.RelayFeedbackValid = false;
    data.RelayGpioCold = false;
    data.RelayGpioHeat = false;
    data.RelayGpioDehum = false;
    data.RelayGpioHum = false;
    data.FanEnabled = false;
    data.FanState = false;
    data.RelayGpioFan = false;
    data.FanPhaseElapsedSeconds = 0;
    data.FanPhaseDurationSeconds = 0;
    data.ColdDebugMsg[0] = '\0';
    data.HeatDebugMsg[0] = '\0';
    data.DehumDebugMsg[0] = '\0';
    data.HumDebugMsg[0] = '\0';

    prefs.begin("MyData",false);
    bool defaultStored = false;

    ensureFloatDefault("SetTemp", 12, defaultStored);
    ensureFloatDefault("SetHum", 70, defaultStored);
    ensureBoolDefault("TempOffsetEn", false, defaultStored);
    ensureFloatDefault("TempOffset", 0, defaultStored);
    ensureBoolDefault("HumOffsetEn", false, defaultStored);
    ensureFloatDefault("HumOffset", 0, defaultStored);
    ensureFloatDefault("ColdHysH", 0.8, defaultStored);
    ensureFloatDefault("ColdHysL", 0.1, defaultStored);
    ensureUIntDefault("ColdMinOn", 60, defaultStored);
    ensureUIntDefault("ColdMinOff", 60, defaultStored);
    ensureFloatDefault("HeatHysH", -0.1, defaultStored);
    ensureFloatDefault("HeatHysL", -0.8, defaultStored);
    ensureUIntDefault("HeatMinOn", 60, defaultStored);
    ensureUIntDefault("HeatMinOff", 60, defaultStored);
    ensureFloatDefault("DehumHysH", 1.5, defaultStored);
    ensureFloatDefault("DehumHysL", 0.5, defaultStored);
    ensureUIntDefault("DehumMinOn", 60, defaultStored);
    ensureUIntDefault("DehumMinOff", 60, defaultStored);
    ensureFloatDefault("HumHysH", -0.5, defaultStored);
    ensureFloatDefault("HumHysL", -1.5, defaultStored);
    ensureUIntDefault("HumMinOn", 60, defaultStored);
    ensureUIntDefault("HumMinOff", 60, defaultStored);
    ensureBoolDefault("FanEnabled", true, defaultStored);
    ensureUIntDefault("FanOnMin", FanDefaultOnMinutes, defaultStored);
    ensureUIntDefault("FanOffMin", FanDefaultOffMinutes, defaultStored);

    // ================== CONSIGNES ==================
    data.SetTemp = prefs.getFloat("SetTemp",12);
    data.SetHum = prefs.getFloat("SetHum",70);

    // ================== CAPTEUR ==================
    data.TempOffsetEnabled = prefs.getBool("TempOffsetEn", false); //bool pour activer ou on l'offset de température
    data.TempOffset = prefs.getFloat("TempOffset",0); // valeur de l'offset de température

    data.HumOffsetEnabled = prefs.getBool("HumOffsetEn", false); //bool pour activer ou on l'offset d'humidité
    data.HumOffset = prefs.getFloat("HumOffset",0); // valeur de l'offset de l'humidité


    // ================== FROID ==================
    data.ColdHysHigh= prefs.getFloat("ColdHysH",0.8); // °C Delta par rapport consigne pour activer froid
    data.ColdHysLow= prefs.getFloat("ColdHysL",0.1); // °C Delta par rapport consigne pour désactiver froid
    data.ColdDelayMinOn= prefs.getUInt("ColdMinOn", 60); // Temps minimum allumé
    data.ColdDelayMinOff= prefs.getUInt("ColdMinOff", 60);// Temps minimum éteint 

    // ================== CHAUD ==================
    data.HeatHysHigh = prefs.getFloat("HeatHysH",-0.1); // °C Delta par rapport consigne pour désactiver chaud
    data.HeatHysLow = prefs.getFloat("HeatHysL",-0.8); // °C Delta par rapport consigne pour activer chaud
    data.HeatDelayMinOn = prefs.getUInt("HeatMinOn", 60);  // Temps minimum allumé (s)
    data.HeatDelayMinOff = prefs.getUInt("HeatMinOff", 60);  // Temps minimum éteint (s)

    // ================== DEHUM ==================
    data.DehumHysHigh= prefs.getFloat("DehumHysH",1.5); // % Delta par rapport consigne pour activer déhumidificateur
    data.DehumHysLow= prefs.getFloat("DehumHysL",0.5); // % Delta par rapport consigne pour désactiver déhumidificateur
    data.DehumDelayMinOn= prefs.getUInt("DehumMinOn", 60); // Temps minimum allumé
    data.DehumDelayMinOff= prefs.getUInt("DehumMinOff", 60);// Temps minimum éteint 

    // ================== HUM ==================
    data.HumHysHigh = prefs.getFloat("HumHysH",-0.5); // % Delta par rapport consigne pour désactiver Humidificateur
    data.HumHysLow = prefs.getFloat("HumHysL",-1.5); // % Delta par rapport consigne pour activer Humidificateur
    data.HumDelayMinOn = prefs.getUInt("HumMinOn", 60);  // Temps minimum allumé (s)
    data.HumDelayMinOff = prefs.getUInt("HumMinOff", 60);  // Temps minimum éteint (s)

    // ================== VENTILATION ==================
    data.FanEnabled = prefs.getBool("FanEnabled", true);
    data.FanOnMinutes = prefs.getUInt("FanOnMin", FanDefaultOnMinutes);
    data.FanOffMinutes = prefs.getUInt("FanOffMin", FanDefaultOffMinutes);

    if (!IsValidTemperatureSetpoint(data.SetTemp)) {
        repairFloat("SetTemp", data.SetTemp, 12.0f);
    }
    if (!IsValidHumiditySetpoint(data.SetHum)) {
        repairFloat("SetHum", data.SetHum, 70.0f);
    }

    if (!IsValidTemperatureOffset(data.TempOffset)) {
        repairFloat("TempOffset", data.TempOffset, 0.0f);
        repairBool("TempOffsetEn", data.TempOffsetEnabled, false);
    }
    if (!IsValidHumidityOffset(data.HumOffset)) {
        repairFloat("HumOffset", data.HumOffset, 0.0f);
        repairBool("HumOffsetEn", data.HumOffsetEnabled, false);
    }

    if (!IsValidTemperatureRelaySettings(data)) {
        repairFloat("ColdHysH", data.ColdHysHigh, 0.8f);
        repairFloat("ColdHysL", data.ColdHysLow, 0.1f);
        repairUInt("ColdMinOn", data.ColdDelayMinOn, 60);
        repairUInt("ColdMinOff", data.ColdDelayMinOff, 60);
        repairFloat("HeatHysH", data.HeatHysHigh, -0.1f);
        repairFloat("HeatHysL", data.HeatHysLow, -0.8f);
        repairUInt("HeatMinOn", data.HeatDelayMinOn, 60);
        repairUInt("HeatMinOff", data.HeatDelayMinOff, 60);
    }

    if (!IsValidHumidityRelaySettings(data)) {
        repairFloat("DehumHysH", data.DehumHysHigh, 1.5f);
        repairFloat("DehumHysL", data.DehumHysLow, 0.5f);
        repairUInt("DehumMinOn", data.DehumDelayMinOn, 60);
        repairUInt("DehumMinOff", data.DehumDelayMinOff, 60);
        repairFloat("HumHysH", data.HumHysHigh, -0.5f);
        repairFloat("HumHysL", data.HumHysLow, -1.5f);
        repairUInt("HumMinOn", data.HumDelayMinOn, 60);
        repairUInt("HumMinOff", data.HumDelayMinOff, 60);
    }

    if (data.FanOnMinutes < FanDurationMinMinutes || data.FanOnMinutes > FanDurationMaxMinutes) {
        repairUInt("FanOnMin", data.FanOnMinutes, FanDefaultOnMinutes);
    }
    if (data.FanOffMinutes < FanDurationMinMinutes || data.FanOffMinutes > FanDurationMaxMinutes) {
        repairUInt("FanOffMin", data.FanOffMinutes, FanDefaultOffMinutes);
    }

    prefs.end();

    if (defaultStored) {
        Serial.println("[NVS] Valeurs par defaut manquantes initialisees.");
    }

    
}

void Get_Json_GlobalStatus(char* buffer, size_t maxSize){

    //Copy local des struct.
    StrctDataController LocalData ;
    StrctDataMqtt LocalMqtt ;

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    LocalData = DataController ;
    LocalMqtt = DataMqtt;
    xSemaphoreGive(DataMutex);


    JsonDocument doc;


    JsonObject Sensor = doc["Sensor"].to<JsonObject>();
    Sensor["Temp"] = LocalData.Temperature;
    Sensor["Hum"]  = LocalData.Humidity;

    JsonObject Consigne = doc["Consigne"].to<JsonObject>();
    Consigne["SetTemp"] = LocalData.SetTemp;
    Consigne["SetHum"]  = LocalData.SetHum;  

    JsonObject Limits = doc["Limits"].to<JsonObject>();
    JsonObject Setpoints = Limits["Setpoints"].to<JsonObject>();
    JsonObject TemperatureSetpoint = Setpoints["Temperature"].to<JsonObject>();
    TemperatureSetpoint["Min"] = MinTemperatureSetpointC;
    TemperatureSetpoint["Max"] = MaxTemperatureSetpointC;
    JsonObject HumiditySetpoint = Setpoints["Humidity"].to<JsonObject>();
    HumiditySetpoint["Min"] = MinHumiditySetpointPercent;
    HumiditySetpoint["Max"] = MaxHumiditySetpointPercent;

    JsonObject SensorOffset = Limits["SensorOffset"].to<JsonObject>();
    JsonObject TemperatureOffset = SensorOffset["Temperature"].to<JsonObject>();
    TemperatureOffset["Min"] = MinTemperatureOffsetC;
    TemperatureOffset["Max"] = MaxTemperatureOffsetC;
    JsonObject HumidityOffset = SensorOffset["Humidity"].to<JsonObject>();
    HumidityOffset["Min"] = MinHumidityOffsetPercent;
    HumidityOffset["Max"] = MaxHumidityOffsetPercent;

    JsonObject Hysteresis = Limits["Hysteresis"].to<JsonObject>();
    JsonObject ColdHysteresis = Hysteresis["Cold"].to<JsonObject>();
    ColdHysteresis["Min"] = MinColdHysteresisC;
    ColdHysteresis["Max"] = MaxColdHysteresisC;
    JsonObject HeatHysteresis = Hysteresis["Heat"].to<JsonObject>();
    HeatHysteresis["Min"] = MinHeatHysteresisC;
    HeatHysteresis["Max"] = MaxHeatHysteresisC;
    JsonObject DehumHysteresis = Hysteresis["Dehum"].to<JsonObject>();
    DehumHysteresis["Min"] = MinDehumHysteresisPercent;
    DehumHysteresis["Max"] = MaxDehumHysteresisPercent;
    JsonObject HumHysteresis = Hysteresis["Hum"].to<JsonObject>();
    HumHysteresis["Min"] = MinHumHysteresisPercent;
    HumHysteresis["Max"] = MaxHumHysteresisPercent;

    JsonObject RelayDelay = Limits["RelayDelaySeconds"].to<JsonObject>();
    RelayDelay["Min"] = MinRelayDelaySeconds;
    RelayDelay["Max"] = MaxRelayDelaySeconds;

    JsonObject FanDuration = Limits["FanDurationMinutes"].to<JsonObject>();
    FanDuration["Min"] = FanDurationMinMinutes;
    FanDuration["Max"] = FanDurationMaxMinutes;

    JsonObject MqttPort = Limits["MqttPort"].to<JsonObject>();
    MqttPort["Min"] = MinMqttPort;
    MqttPort["Max"] = MaxMqttPort;

    JsonObject AlarmThreshold = Limits["AlarmThresholdC"].to<JsonObject>();
    AlarmThreshold["Min"] = SafetyAlarmThresholdMinC;
    AlarmThreshold["Max"] = SafetyAlarmThresholdMaxC;

    JsonObject SensorConfig = doc["SensorConfig"].to<JsonObject>();
    SensorConfig["TempOffsetEnabled"] = LocalData.TempOffsetEnabled;
    SensorConfig["TempOffset"] = LocalData.TempOffset;
    SensorConfig["HumOffsetEnabled"] = LocalData.HumOffsetEnabled;
    SensorConfig["HumOffset"] = LocalData.HumOffset;


    JsonObject State = doc["State"].to<JsonObject>();
    State["StateCold"]  = LocalData.ColdState;
    State["StateHeat"]  = LocalData.HeatState;
    State["StateDehum"] = LocalData.DehumState;
    State["StateHum"] = LocalData.HumState;
    State["GpioFeedbackValid"] = LocalData.RelayFeedbackValid;
    JsonArray GpioFeedback = State["GpioFeedback"].to<JsonArray>();
    GpioFeedback.add(LocalData.RelayGpioCold);
    GpioFeedback.add(LocalData.RelayGpioHeat);
    GpioFeedback.add(LocalData.RelayGpioDehum);
    GpioFeedback.add(LocalData.RelayGpioHum);
    GpioFeedback.add(LocalData.RelayGpioFan);

    JsonObject Fan = doc["Fan"].to<JsonObject>();
    Fan["Enabled"] = LocalData.FanEnabled;
    Fan["State"] = LocalData.FanState;
    Fan["OnMinutes"] = LocalData.FanOnMinutes;
    Fan["OffMinutes"] = LocalData.FanOffMinutes;
    Fan["ElapsedSeconds"] = LocalData.FanPhaseElapsedSeconds;
    Fan["DurationSeconds"] = LocalData.FanPhaseDurationSeconds;


    JsonObject HardwareCold = doc["HardwareCold"].to<JsonObject>();
    HardwareCold["HysHigh"] = LocalData.ColdHysHigh;
    HardwareCold["HysLow"] = LocalData.ColdHysLow;
    HardwareCold["DelayMinOn"] = LocalData.ColdDelayMinOn;
    HardwareCold["DelayMinOff"] = LocalData.ColdDelayMinOff;

    JsonObject HardwareHeat = doc["HardwareHeat"].to<JsonObject>();
    HardwareHeat["HysHigh"] = LocalData.HeatHysHigh;
    HardwareHeat["HysLow"] = LocalData.HeatHysLow;
    HardwareHeat["DelayMinOn"] = LocalData.HeatDelayMinOn;
    HardwareHeat["DelayMinOff"] = LocalData.HeatDelayMinOff;

    JsonObject HardwareDehum = doc["HardwareDehum"].to<JsonObject>();
    HardwareDehum["HysHigh"] = LocalData.DehumHysHigh;
    HardwareDehum["HysLow"] = LocalData.DehumHysLow;
    HardwareDehum["DelayMinOn"] = LocalData.DehumDelayMinOn;
    HardwareDehum["DelayMinOff"] = LocalData.DehumDelayMinOff;

    JsonObject HardwareHum = doc["HardwareHum"].to<JsonObject>();
    HardwareHum["HysHigh"] = LocalData.HumHysHigh;
    HardwareHum["HysLow"] = LocalData.HumHysLow;
    HardwareHum["DelayMinOn"] = LocalData.HumDelayMinOn;
    HardwareHum["DelayMinOff"] = LocalData.HumDelayMinOff;

    JsonObject DebugMsg = doc["DebugMsg"].to<JsonObject>();
    DebugMsg["ColdDebugMsg"]  = LocalData.ColdDebugMsg;
    DebugMsg["HeatDebugMsg"]  = LocalData.HeatDebugMsg;
    DebugMsg["DehumDebugMsg"] = LocalData.DehumDebugMsg;
    DebugMsg["HumDebugMsg"]   = LocalData.HumDebugMsg;


    bool Connected = WiFi.status() == WL_CONNECTED;
    String Ssid = WiFi.SSID();
    int Rssi = WiFi.RSSI();
    String Ip = WiFi.localIP().toString();

    JsonObject Wifi = doc["Wifi"].to<JsonObject>();
    Wifi["Connected"] = Connected;
    Wifi["Ssid"]      = Ssid;
    Wifi["Rssi"]      = Rssi;
    Wifi["Ip"]        = Ip;


    JsonObject Mqtt = doc["Mqtt"].to<JsonObject>();
    Mqtt["MqttEnable"] = LocalMqtt.MqttEnable;
    Mqtt["mqtt_server"] = LocalMqtt.MqttServer;
    Mqtt["mqtt_server_port"] = LocalMqtt.MqttPort;
    Mqtt["MQTTUSER"] = LocalMqtt.MqttUser;
    Mqtt["NAME"] = LocalMqtt.MqttDeviceName;
    Mqtt["Connected"] = LocalMqtt.MqttConnected;


   // serializeJson(doc, buffer,maxSize);

    size_t n = serializeJson(doc, buffer, maxSize);

   // Serial.println("--- Rapport de Taille JSON ---");
   // Serial.printf("Octets écrits : %u / %u\n", n, maxSize);

    // Si n est égal à maxSize - 1, c'est que le JSON a probablement été coupé !
    if (n >= maxSize - 1) {
        Serial.println("⚠️ ALERTE : Le buffer est TROP PETIT, le JSON est tronqué !");
    }

    // Optionnel : mesurer combien d'espace le document occupe en mémoire RAM
   // Serial.printf("Mémoire document JSON : %u octets\n", doc.memoryUsage());

}



bool Save_SetTemp_and_SetHum(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour les consignes.");
        return false;
    }
    bool saved = true;
    if (prefs.putFloat("SetTemp", DataController.SetTemp) != sizeof(DataController.SetTemp)) saved = false;
    if (prefs.putFloat("SetHum", DataController.SetHum) != sizeof(DataController.SetHum)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex);  
    if (!saved) Serial.println("[NVS] Echec d'enregistrement des consignes.");
    return saved;
}

void Save_Wifi_Parameters(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    prefs.begin("MyWifi", false);
    prefs.putString("Ssid", DataWifi.WifiSsid);
    prefs.putString("Pass", DataWifi.WifiPass);
    prefs.end();
    xSemaphoreGive(DataMutex);  
}

void Save_Mqtt_Parameters(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    prefs.begin("MyMqtt", false);
    prefs.putBool("MqttEnable",    DataMqtt.MqttEnable);
    prefs.putString("MqttServer", DataMqtt.MqttServer);         // MQTT Server URL
    prefs.putUInt("MqttPort", DataMqtt.MqttPort);// MQTT Server port
    prefs.putString("MqttUser", DataMqtt.MqttUser);             // MQTT Username
    prefs.putString("MqttPass", DataMqtt.MqttPassword);         //MQTT Password
    prefs.putString("MqttName", DataMqtt.MqttDeviceName);               // Device name in Home Assistant
    prefs.end();
    xSemaphoreGive(DataMutex); 
}

bool Save_SensorOffset(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour la calibration.");
        return false;
    }
    bool saved = true;
    if (prefs.putBool("TempOffsetEn", DataController.TempOffsetEnabled) != sizeof(DataController.TempOffsetEnabled)) saved = false;
    if (prefs.putFloat("TempOffset", DataController.TempOffset) != sizeof(DataController.TempOffset)) saved = false;
    if (prefs.putBool("HumOffsetEn", DataController.HumOffsetEnabled) != sizeof(DataController.HumOffsetEnabled)) saved = false;
    if (prefs.putFloat("HumOffset", DataController.HumOffset) != sizeof(DataController.HumOffset)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex); 
    if (!saved) Serial.println("[NVS] Echec d'enregistrement de la calibration.");
    return saved;
}

bool Save_ColdHardware(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour le froid.");
        return false;
    }
    bool saved = true;
    if (prefs.putFloat("ColdHysH", DataController.ColdHysHigh) != sizeof(DataController.ColdHysHigh)) saved = false;
    if (prefs.putFloat("ColdHysL", DataController.ColdHysLow) != sizeof(DataController.ColdHysLow)) saved = false;
    if (prefs.putUInt("ColdMinOn", DataController.ColdDelayMinOn) != sizeof(DataController.ColdDelayMinOn)) saved = false;
    if (prefs.putUInt("ColdMinOff", DataController.ColdDelayMinOff) != sizeof(DataController.ColdDelayMinOff)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex); 
    if (!saved) Serial.println("[NVS] Echec d'enregistrement des paramètres froid.");
    return saved;
}
bool Save_HeatHardware(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour le chauffage.");
        return false;
    }
    bool saved = true;
    if (prefs.putFloat("HeatHysH", DataController.HeatHysHigh) != sizeof(DataController.HeatHysHigh)) saved = false;
    if (prefs.putFloat("HeatHysL", DataController.HeatHysLow) != sizeof(DataController.HeatHysLow)) saved = false;
    if (prefs.putUInt("HeatMinOn", DataController.HeatDelayMinOn) != sizeof(DataController.HeatDelayMinOn)) saved = false;
    if (prefs.putUInt("HeatMinOff", DataController.HeatDelayMinOff) != sizeof(DataController.HeatDelayMinOff)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex); 
    if (!saved) Serial.println("[NVS] Echec d'enregistrement des paramètres chauffage.");
    return saved;
}
bool Save_DehumHardware(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour la deshumidification.");
        return false;
    }
    bool saved = true;
    if (prefs.putFloat("DehumHysH", DataController.DehumHysHigh) != sizeof(DataController.DehumHysHigh)) saved = false;
    if (prefs.putFloat("DehumHysL", DataController.DehumHysLow) != sizeof(DataController.DehumHysLow)) saved = false;
    if (prefs.putUInt("DehumMinOn", DataController.DehumDelayMinOn) != sizeof(DataController.DehumDelayMinOn)) saved = false;
    if (prefs.putUInt("DehumMinOff", DataController.DehumDelayMinOff) != sizeof(DataController.DehumDelayMinOff)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex); 
    if (!saved) Serial.println("[NVS] Echec d'enregistrement des paramètres deshumidification.");
    return saved;
}
bool Save_HumHardware(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);  
    if (!prefs.begin("MyData", false)) {
        xSemaphoreGive(DataMutex);
        Serial.println("[NVS] Echec d'ouverture de MyData pour l'humidification.");
        return false;
    }
    bool saved = true;
    if (prefs.putFloat("HumHysH", DataController.HumHysHigh) != sizeof(DataController.HumHysHigh)) saved = false;
    if (prefs.putFloat("HumHysL", DataController.HumHysLow) != sizeof(DataController.HumHysLow)) saved = false;
    if (prefs.putUInt("HumMinOn", DataController.HumDelayMinOn) != sizeof(DataController.HumDelayMinOn)) saved = false;
    if (prefs.putUInt("HumMinOff", DataController.HumDelayMinOff) != sizeof(DataController.HumDelayMinOff)) saved = false;
    prefs.end();
    xSemaphoreGive(DataMutex); 
    if (!saved) Serial.println("[NVS] Echec d'enregistrement des paramètres humidification.");
    return saved;
}

void Save_FanSettings(){
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    prefs.begin("MyData", false);
    prefs.putBool("FanEnabled", DataController.FanEnabled);
    prefs.putUInt("FanOnMin", DataController.FanOnMinutes);
    prefs.putUInt("FanOffMin", DataController.FanOffMinutes);
    prefs.end();
    xSemaphoreGive(DataMutex);
}
