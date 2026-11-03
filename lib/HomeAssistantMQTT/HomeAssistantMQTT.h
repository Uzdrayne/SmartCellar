#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define HAMQTT_MAXITEMS 30

struct ItemValue
{
  char* item;
  char* value;
};

class HomeAssistantMQTT
{
  private:
    PubSubClient* mqttClient = nullptr;

    char* MqttStateTopic = nullptr; // <Manufacturer>/<DeviceName>/state
    char* StateTopic = nullptr;     // <Manufacturer>/<DeviceName>

    ItemValue* values[HAMQTT_MAXITEMS];
	
	
    unsigned long _nextMqttAttemptMs = 0;
    unsigned long _mqttBackoffMs = 1000;    // 1s au début
    const unsigned long _mqttBackoffMaxMs = 30000; // 30s max


    void connect();
    void publishConfig(const char* type, String category, String deviceClass, String stateClass, String name, String icon, String unit, bool commandTopic, bool stateTopic, String commandTopicName, String complement, String startupValue);
    void MqttCallback(char* topic, byte* payload, unsigned int length);

    typedef void (*CallbackFunction) (String item, String payload, bool readFromMQTT);
    CallbackFunction cb_callback = NULL;

  public:
    String MqttUser;
    String MqttPassword;
  
    String Manufacturer;
    String Model;
    String Version;
    String Name;
    String DeviceName;
  
    HomeAssistantMQTT();
    void begin(WiFiClient* wifiClient, const char* server, const uint16_t port);
    void loop();

    bool connected();
    void readValues();
    void sendValues();
    void setCallback(CallbackFunction f);

    void setValue(String item, String value);
    String getValue(String item);

    void publishConfigSensor(String deviceClass, String stateClass, String name, String icon, String unit, String startupValue);
    void publishConfigBinarySensor(String deviceClass, String name, String icon, String payloadOff, String payloadOn, String startupValue);
    void publishConfigNumber(String category, String name, String icon, String unit, String min, String max, String startupValue, String step = "0.1");
    void publishConfigButton(String category, String name, String icon, String commandTopicName, String payload);
    void publishConfigSelect(String category, String name, String icon, String options[], unsigned short optionsCount, String startupValue);
    void publishConfigSwitch(String category, String name, String icon, String startupValue);
};
