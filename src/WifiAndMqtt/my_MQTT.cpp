#include <Arduino.h>
#include <WiFi.h>
#include <stdlib.h>

#include <HomeAssistantMQTT.h>

#include "Data/ControllerData.h"
#include "Controller/ControllerValidation.h"


//#define  DEBUG

// Forward declarations
void HAMQTT_Callback(String item, String payload, bool readFromMQTT);
void publishMqttConfig();
void publishMqttValues(unsigned long Now);


//indique qu'on à deja envoyé l'autoDiscovery
bool _bMqttConfigPublished = false;

//Temps dernier envoie d'un message
unsigned long  mqttLastSend = 0; 
//temps minimum entre deux messages
unsigned long  mqttTimeSend = 2000;

//temps maximum avant de renvoyé la valeur d'un item
unsigned long ItemTimeOut = 300000; //5min


//valeur actuelles
StrctDataController LocalData ;
float ActualTemperatureCorr = 0;
float ActualHumidityCorr =0;

//LastValeur
StrctDataController LastLocalData ;
float LastActualTemperatureCorr = 0;
float LastActualHumidityCorr =0;


//LastSendValue
unsigned long  T_TempOffsetEnabled = 0;
unsigned long  T_TempOffset = 0;
unsigned long  T_HumOffsetEnabled = 0;
unsigned long  T_HumOffset = 0;
unsigned long  T_SetTemp = 0;
unsigned long  T_SetHum = 0;
unsigned long  T_ColdState = 0;
unsigned long  T_HeatState = 0;
unsigned long  T_DehumState = 0;
unsigned long  T_HumState = 0;
unsigned long  T_FanState = 0;
unsigned long  T_FanEnabled = 0;
unsigned long  T_FanOnMinutes = 0;
unsigned long  T_FanOffMinutes = 0;
unsigned long  T_ActualTemperatureCorr = 0;
unsigned long  T_ActualHumidityCorr = 0;


//fonction pour se connecter
void ConnectMqtt(){

    String mac = WiFi.macAddress();
    // Remove ':' to build a compact MAC string
    String MAC = mac.substring(0, 2) + mac.substring(3, 5) + mac.substring(6, 8) + mac.substring(9, 11) + mac.substring(12, 14) + mac.substring(15, 17);

    // Configure MQTT wrapper
    mqtt.MqttUser = DataMqtt.MqttUser;
    mqtt.MqttPassword = DataMqtt.MqttPassword;
    mqtt.Manufacturer = DataMqtt.MqttManu;
    mqtt.Model = DataMqtt.MqttModel;
    mqtt.Version = DataMqtt.MqttVersion;
    mqtt.Name = DataMqtt.MqttDeviceName;
    mqtt.DeviceName = DataMqtt.MqttManu + "_" + DataMqtt.MqttModel + "_" + MAC;
    mqtt.setCallback(HAMQTT_Callback);
    mqtt.begin(&wifiClient, DataMqtt.MqttServer.c_str(), DataMqtt.MqttPort);

}

//loop (appeler dans la main loop)
void MyMqttLoop(){

    unsigned long now=millis();

    Set_MqttConnected(mqtt.connected());
   
	// If MQTT is disconnected, mark config as not published so it'll be republished on next connect
	if (!mqtt.connected()) {
		_bMqttConfigPublished = false;
	}

	mqtt.loop();

	if (mqtt.connected() && !_bMqttConfigPublished)
	{
		publishMqttConfig();// Publish HA discovery config
		mqtt.sendValues();// Publish initial state so HA sees entities immediately
		//mqtt.readValues();// Try to read retained values to restore previous state
		_bMqttConfigPublished = true;
	}

    //Si délais supérieur à mqttTimeSend
    if (mqtt.connected() && (now - mqttLastSend) >= mqttTimeSend) {
	    mqttLastSend = now;
	    publishMqttValues(now); //envoyer les valeurs ( si differentes )
    }

}

//envoyer les infos pour l'auto discovery home assistant
void publishMqttConfig()
{
#ifdef DEBUG
  Serial.println("Publishing config:");
#endif

    //copie des valeur dans un local 
    
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    LocalData = DataController;
    xSemaphoreGive(DataMutex);

    ActualTemperatureCorr = LocalData.Temperature +  (LocalData.TempOffsetEnabled ? LocalData.TempOffset : 0.0f);
    ActualHumidityCorr = LocalData.Humidity + (LocalData.HumOffsetEnabled ? LocalData.HumOffset : 0.0f);

    mqtt.publishConfigSensor("temperature", "measurement", "Temperature", "", "°C", String(ActualTemperatureCorr));
    mqtt.publishConfigSensor("humidity", "measurement", "Humidity", "", "%", String(ActualHumidityCorr));

    mqtt.publishConfigNumber("config", "ConsigneTemperature", "mdi:Thermometer-Lines", "°C", "5", "30", String(LocalData.SetTemp));
    mqtt.publishConfigNumber("config", "ConsigneHumidity", "mdi:water-percent", "%", "40", "100", String(LocalData.SetHum));

    mqtt.publishConfigBinarySensor("", "Cold_Relay", "", "false", "true", LocalData.ColdState ? "true" : "false");
    mqtt.publishConfigBinarySensor("", "Heat_Relay", "", "false", "true", LocalData.HeatState ? "true" : "false");
    mqtt.publishConfigBinarySensor("", "Dehum_Relay", "", "false", "true", LocalData.DehumState ? "true" : "false");
    mqtt.publishConfigBinarySensor("", "Hum_Relay", "", "false", "true", LocalData.HumState ? "true" : "false");
    mqtt.publishConfigBinarySensor("", "Ventilation_Relay", "mdi:fan", "false", "true", LocalData.FanState ? "true" : "false");
    mqtt.publishConfigSwitch("config", "Ventilation_Enabled", "mdi:fan-auto", LocalData.FanEnabled ? "true" : "false");
    mqtt.publishConfigNumber("config", "Ventilation_On_Minutes", "mdi:timer-play", "min", String(FanDurationMinMinutes), String(FanDurationMaxMinutes), String(LocalData.FanOnMinutes), "1");
    mqtt.publishConfigNumber("config", "Ventilation_Off_Minutes", "mdi:timer-pause", "min", String(FanDurationMinMinutes), String(FanDurationMaxMinutes), String(LocalData.FanOffMinutes), "1");


    //mqtt.publishConfigButton("", "Open", "mdi:window-shutter-open", "Command", "open");
    //String options[] = { "Open", "Close", "None" };
    //mqtt.publishConfigSelect("config", "Startup init", "mdi:cog-refresh", options, 3, "Open");
    //mqtt.publishConfigSwitch("config", "Enabled", "mdi:blur", "true");
  

}

//envoyé les valeurs à HA (si nouvelles ou si > timeOut)
void publishMqttValues(unsigned long Now){


    //copie des valeur dans un local 
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    LocalData = DataController;
    xSemaphoreGive(DataMutex);



    ActualTemperatureCorr = LocalData.Temperature +  (LocalData.TempOffsetEnabled ? LocalData.TempOffset : 0.0f);
    ActualHumidityCorr = LocalData.Humidity + (LocalData.HumOffsetEnabled ? LocalData.HumOffset : 0.0f);

    //Si valeur diffenente de la derniere envoyée / ou si Temps de la derniere valeur envoyé > TimeOut
    if((ActualTemperatureCorr != LastActualTemperatureCorr) || (Now - T_ActualTemperatureCorr) >= ItemTimeOut){
        mqtt.setValue("Temperature", String(ActualTemperatureCorr));
        LastActualTemperatureCorr = ActualTemperatureCorr; 
        T_ActualTemperatureCorr = Now;
    } 
    
    if((ActualHumidityCorr != LastActualHumidityCorr) || (Now - T_ActualHumidityCorr) >= ItemTimeOut){
        mqtt.setValue("Humidity", String(ActualHumidityCorr));
        LastActualHumidityCorr = ActualHumidityCorr; 
        T_ActualHumidityCorr = Now;
    } 
    
    if((LocalData.SetTemp != LastLocalData.SetTemp) || (Now - T_SetTemp) >= ItemTimeOut){
        mqtt.setValue("ConsigneTemperature", String(LocalData.SetTemp));
        LastLocalData.SetTemp = LocalData.SetTemp; 
        T_SetTemp = Now;
    } 
    
    if((LocalData.SetHum != LastLocalData.SetHum) || (Now - T_SetHum) >= ItemTimeOut){
        mqtt.setValue("ConsigneHumidity", String(LocalData.SetHum));
        LastLocalData.SetHum = LocalData.SetHum; 
        T_SetHum = Now;
    } 


    if((LocalData.ColdState != LastLocalData.ColdState) || (Now - T_ColdState) >= ItemTimeOut){
        mqtt.setValue("Cold_Relay", LocalData.ColdState ? "true" : "false");
        LastLocalData.ColdState = LocalData.ColdState; 
        T_ColdState = Now;
    } 

    if((LocalData.HeatState != LastLocalData.HeatState) || (Now - T_HeatState) >= ItemTimeOut){
        mqtt.setValue("Heat_Relay", LocalData.HeatState ? "true" : "false");
        LastLocalData.HeatState = LocalData.HeatState; 
        T_HeatState = Now;
    }     

    if((LocalData.DehumState != LastLocalData.DehumState) || (Now - T_DehumState) >= ItemTimeOut){
        mqtt.setValue("Dehum_Relay", LocalData.DehumState ? "true" : "false");
        LastLocalData.DehumState = LocalData.DehumState; 
        T_DehumState = Now;
    } 

    if((LocalData.HumState != LastLocalData.HumState) || (Now - T_HumState) >= ItemTimeOut){
        mqtt.setValue("Hum_Relay", LocalData.HumState ? "true" : "false");
        LastLocalData.HumState = LocalData.HumState; 
        T_HumState = Now;
    }     

    if((LocalData.FanState != LastLocalData.FanState) || (Now - T_FanState) >= ItemTimeOut){
        mqtt.setValue("Ventilation_Relay", LocalData.FanState ? "true" : "false");
        LastLocalData.FanState = LocalData.FanState;
        T_FanState = Now;
    }

    if((LocalData.FanEnabled != LastLocalData.FanEnabled) || (Now - T_FanEnabled) >= ItemTimeOut){
        mqtt.setValue("Ventilation_Enabled", LocalData.FanEnabled ? "true" : "false");
        LastLocalData.FanEnabled = LocalData.FanEnabled;
        T_FanEnabled = Now;
    }

    if((LocalData.FanOnMinutes != LastLocalData.FanOnMinutes) || (Now - T_FanOnMinutes) >= ItemTimeOut){
        mqtt.setValue("Ventilation_On_Minutes", String(LocalData.FanOnMinutes));
        LastLocalData.FanOnMinutes = LocalData.FanOnMinutes;
        T_FanOnMinutes = Now;
    }

    if((LocalData.FanOffMinutes != LastLocalData.FanOffMinutes) || (Now - T_FanOffMinutes) >= ItemTimeOut){
        mqtt.setValue("Ventilation_Off_Minutes", String(LocalData.FanOffMinutes));
        LastLocalData.FanOffMinutes = LocalData.FanOffMinutes;
        T_FanOffMinutes = Now;
    }


	mqtt.sendValues();
		

}

//Message venant de MQTT
void HAMQTT_Callback(String item, String payload, bool readFromMQTT){

    #ifdef DEBUG
    Serial.print("Message arrived for item: '");
    Serial.print(item);
    Serial.print("' with payload: ");
    Serial.print(payload);
    if (readFromMQTT)
        Serial.print(" (read from MQTT)");
    Serial.println();
    #endif

  // PROCESS NEW DATA HERE
  if (item != "Command")
  {

    if(item == "ConsigneTemperature")
    {
        float val;
        if (!ParseFiniteFloat(payload, val) || !IsValidTemperatureSetpoint(val)) {
            Serial.printf("MQTT : consigne temperature invalide: %s\n", payload.c_str());
            return;
        }

        xSemaphoreTake(DataMutex, portMAX_DELAY);
        const bool changed = DataController.SetTemp != val;
        DataController.SetTemp = val;
        xSemaphoreGive(DataMutex);
        if (changed) {
            Serial.printf("MQTT : SetTemp updated to %.2f\n", val);
        }

    }
    else if(item == "ConsigneHumidity")
    {
        float val;
        if (!ParseFiniteFloat(payload, val) || !IsValidHumiditySetpoint(val)) {
            Serial.printf("MQTT : consigne humidite invalide: %s\n", payload.c_str());
            return;
        }

        xSemaphoreTake(DataMutex, portMAX_DELAY);
        const bool changed = DataController.SetHum != val;
        DataController.SetHum = val;
        xSemaphoreGive(DataMutex);
        if (changed) {
            Serial.printf("MQTT : SetHum updated to %.2f\n", val);
        }
    }
    else if(item == "Ventilation_Enabled")
    {
        payload.trim();
        bool value;
        if (payload == "true" || payload == "ON") {
            value = true;
        } else if (payload == "false" || payload == "OFF") {
            value = false;
        } else {
            Serial.printf("MQTT : valeur Ventilation_Enabled invalide: %s\n", payload.c_str());
            return;
        }

        xSemaphoreTake(DataMutex, portMAX_DELAY);
        const bool changed = DataController.FanEnabled != value;
        DataController.FanEnabled = value;
        xSemaphoreGive(DataMutex);
        if (changed) {
            Save_FanSettings();
            Serial.printf("MQTT : Ventilation %s\n", value ? "activee" : "desactivee");
        }
    }
    else if(item == "Ventilation_On_Minutes" || item == "Ventilation_Off_Minutes")
    {
        payload.trim();
        const char *text = payload.c_str();
        char *end = nullptr;
        const long value = strtol(text, &end, 10);
        if (text == end || *end != '\0' || value < FanDurationMinMinutes || value > FanDurationMaxMinutes) {
            Serial.printf("MQTT : duree de ventilation invalide pour %s: %s\n", item.c_str(), payload.c_str());
            return;
        }

        xSemaphoreTake(DataMutex, portMAX_DELAY);
        unsigned int &setting = item == "Ventilation_On_Minutes"
            ? DataController.FanOnMinutes
            : DataController.FanOffMinutes;
        const bool changed = setting != static_cast<unsigned int>(value);
        setting = static_cast<unsigned int>(value);
        xSemaphoreGive(DataMutex);
        if (changed) {
            Save_FanSettings();
            Serial.printf("MQTT : %s = %ld min\n", item.c_str(), value);
        }
    }


  }
}
