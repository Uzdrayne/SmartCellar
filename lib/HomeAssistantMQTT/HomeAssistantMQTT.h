#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

constexpr size_t HAMQTT_MAXITEMS = 24;
constexpr size_t HAMQTT_ITEM_NAME_SIZE = 32;
constexpr size_t HAMQTT_ITEM_VALUE_SIZE = 96;
constexpr size_t HAMQTT_TOPIC_SIZE = 128;

// Description d'une entité Home Assistant (découverte MQTT).
struct HaEntityConfig {
  const char* component;    // "sensor", "binary_sensor", "number", "switch"
  const char* name;         // Clé dans l'état JSON et suffixe des topics
  const char* category;     // nullptr ou "config"
  const char* deviceClass;  // nullptr si aucune
  const char* stateClass;   // nullptr si aucune
  const char* icon;         // nullptr si aucune
  const char* unit;         // nullptr si aucune
  bool command;             // Abonnement à <Manufacturer>/<DeviceName>/set/<name>
  bool displayPrecision;    // suggested_display_precision = 2
  float min;                // number uniquement
  float max;
  float step;
};

class HomeAssistantMQTT
{
  public:
    // item = nom de l'entité, payload = texte reçu (terminé par '\0').
    typedef void (*CallbackFunction)(const char* item, const char* payload);

    String MqttUser;
    String MqttPassword;

    String Manufacturer;
    String Model;
    String Version;
    String Name;
    String DeviceName;

    void begin(WiFiClient* wifiClient, const char* server, uint16_t port);
    void loop();
    bool connected();
    void setCallback(CallbackFunction f);
    void setNanoAvailable(bool available);

    void publishDiscovery(const HaEntityConfig& entity);
    // Renvoie true si la valeur a changé depuis la dernière publication.
    bool setValue(const char* item, const char* value);
    // Publie l'état complet si une valeur a changé, ou si force est vrai.
    bool sendValues(bool force = false);

  private:
    struct ItemValue {
      char item[HAMQTT_ITEM_NAME_SIZE];
      char value[HAMQTT_ITEM_VALUE_SIZE];
    };

    PubSubClient* mqttClient = nullptr;
    // PubSubClient ne copie pas le nom d'hôte : il doit vivre aussi longtemps que le client.
    char serverHost[HAMQTT_TOPIC_SIZE] = {};
    char availabilityTopic[HAMQTT_TOPIC_SIZE] = {};  // <Manufacturer>/<DeviceName>/state
    char nanoAvailabilityTopic[HAMQTT_TOPIC_SIZE] = {};
    char stateTopic[HAMQTT_TOPIC_SIZE] = {};         // <Manufacturer>/<DeviceName>
    char commandTopicPrefix[HAMQTT_TOPIC_SIZE] = {}; // <Manufacturer>/<DeviceName>/set/

    ItemValue values[HAMQTT_MAXITEMS] = {};
    size_t valueCount = 0;
    bool valuesDirty = false;

    unsigned long _nextMqttAttemptMs = 0;
    unsigned long _lastAvailabilityPublishMs = 0;
    unsigned long _lastNanoAvailabilityPublishMs = 0;
    unsigned long _mqttBackoffMs = 1000;
    const unsigned long _mqttBackoffMaxMs = 30000;
    bool _nanoAvailabilityPublished = false;
    bool _nanoAvailable = false;

    CallbackFunction cb_callback = nullptr;

    void connect();
    void MqttCallback(char* topic, byte* payload, unsigned int length);
};
