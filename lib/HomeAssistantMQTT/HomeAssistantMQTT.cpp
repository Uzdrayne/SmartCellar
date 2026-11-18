#include "HomeAssistantMQTT.h"

#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <math.h>

#define CFG_ON_SERIAL

namespace {

// Les messages sont sérialisés ici puis envoyés en une seule écriture :
// pas de String temporaire, et le tampon interne de PubSubClient reste petit.
constexpr size_t PublishBufferSize = 1024;
char publishBuffer[PublishBufferSize];

constexpr uint16_t ReceiveBufferSize = 512;
constexpr size_t MaxCommandPayloadSize = 64;
constexpr uint16_t EntityStateExpireAfterSeconds = 600;
constexpr unsigned long AvailabilityHeartbeatIntervalMs = 30000;
constexpr char OnlineAvailabilityPayload[] = "{\"state\":\"online\"}";

unsigned long lastMqttPublishFailureLog = 0;

void logMqttPublishFailure(const char* operation, size_t payloadLength,
                           PubSubClient* client)
{
  const unsigned long now = millis();
  if (now - lastMqttPublishFailureLog < 1000)
    return;

  lastMqttPublishFailureLog = now;
  const bool connected = client->connected();
  Serial.printf(
      "[MQTT] %s échoué (%u octets, état %d, connecté %u, Wi-Fi %u, heap %u, bloc %u)\n",
      operation, static_cast<unsigned int>(payloadLength), client->state(),
      connected ? 1U : 0U, WiFi.status() == WL_CONNECTED ? 1U : 0U,
      static_cast<unsigned int>(ESP.getFreeHeap()),
      static_cast<unsigned int>(
          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

bool publishJson(PubSubClient* client, const char* topic,
                 const JsonDocument& document, const char* operation,
                 bool retained)
{
  const size_t length = measureJson(document);
  if (length >= PublishBufferSize) {
    Serial.printf("[MQTT] %s trop volumineux (%u octets)\n", operation,
                  static_cast<unsigned int>(length));
    return false;
  }
  serializeJson(document, publishBuffer, PublishBufferSize);

  const bool sent = client->beginPublish(topic, length, retained) &&
                    client->write(reinterpret_cast<const uint8_t*>(publishBuffer),
                                  length) == length &&
                    client->endPublish();
  if (!sent) {
    logMqttPublishFailure(operation, length, client);
  }
  return sent;
}

bool hasText(const char* text)
{
  return text != nullptr && text[0] != '\0';
}

bool dependsOnNanoTelemetry(const HaEntityConfig& entity)
{
  if (strcmp(entity.component, "sensor") == 0) {
    return strcmp(entity.name, "Temperature") == 0 ||
           strcmp(entity.name, "Humidity") == 0;
  }

  return strcmp(entity.component, "binary_sensor") == 0 &&
         strstr(entity.name, "_Relay") != nullptr;
}

}

void HomeAssistantMQTT::setCallback(CallbackFunction f)
{
  cb_callback = f;
}

void HomeAssistantMQTT::begin(WiFiClient* wifiClient, const char* server, const uint16_t port)
{
  if (mqttClient != nullptr) return;

  snprintf(stateTopic, sizeof(stateTopic), "%s/%s",
           Manufacturer.c_str(), DeviceName.c_str());
  snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/state", stateTopic);
  snprintf(nanoAvailabilityTopic, sizeof(nanoAvailabilityTopic), "%s/nano/state", stateTopic);
  snprintf(commandTopicPrefix, sizeof(commandTopicPrefix), "%s/set/", stateTopic);

  snprintf(serverHost, sizeof(serverHost), "%s", server != nullptr ? server : "");
  mqttClient = new PubSubClient(*wifiClient);
  mqttClient->setBufferSize(ReceiveBufferSize);
  mqttClient->setServer(serverHost, port);
  mqttClient->setCallback(std::bind(&HomeAssistantMQTT::MqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  mqttClient->setKeepAlive(5);
  mqttClient->setSocketTimeout(2);
}

void HomeAssistantMQTT::loop()
{
  if (mqttClient == nullptr) return;

  if (!mqttClient->connected())
    connect();

  mqttClient->loop();
  if (mqttClient->connected()) {
    const unsigned long now = millis();
    if (now - _lastAvailabilityPublishMs >= AvailabilityHeartbeatIntervalMs) {
      if (mqttClient->publish(availabilityTopic, OnlineAvailabilityPayload, false)) {
        _lastAvailabilityPublishMs = now;
      } else {
        logMqttPublishFailure("battement disponibilité", sizeof(OnlineAvailabilityPayload) - 1,
                              mqttClient);
      }
    }
  }
}

void HomeAssistantMQTT::connect()
{
  if (mqttClient == nullptr) return;

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient->connected()) return;

  const unsigned long now = millis();
  if (static_cast<long>(now - _nextMqttAttemptMs) < 0) return;

#ifdef CFG_ON_SERIAL
  Serial.println("Attempting MQTT connection...");
#endif

  const bool ok = mqttClient->connect(
      DeviceName.c_str(),
      MqttUser.c_str(), MqttPassword.c_str(),
      availabilityTopic, 1, true, "{\"state\":\"offline\"}"
  );

  if (ok) {
    if (!mqttClient->publish(stateTopic, "", true)) {
      logMqttPublishFailure("suppression de l'état retenu", 0, mqttClient);
    }
    if (!mqttClient->publish(nanoAvailabilityTopic, "", true)) {
      logMqttPublishFailure("suppression de la disponibilité Nano retenue", 0, mqttClient);
    }
    _nanoAvailabilityPublished = false;
    if (!mqttClient->publish(availabilityTopic, "", true)) {
      logMqttPublishFailure("suppression de la disponibilité retenue", 0, mqttClient);
    }
    if (!mqttClient->publish(availabilityTopic, OnlineAvailabilityPayload, false)) {
      logMqttPublishFailure("publication disponibilité", sizeof(OnlineAvailabilityPayload) - 1,
                            mqttClient);
    }
    _lastAvailabilityPublishMs = millis();
#ifdef CFG_ON_SERIAL
    Serial.println("MQTT connected");
#endif
    _mqttBackoffMs = 1000;
    _nextMqttAttemptMs = now;
    // Republie l'état complet après chaque reconnexion.
    valuesDirty = true;
  } else {
#ifdef CFG_ON_SERIAL
    Serial.printf("MQTT failed, rc=%d - retry in %lu ms\n",
                  mqttClient->state(), _mqttBackoffMs);
#endif
    _nextMqttAttemptMs = now + _mqttBackoffMs;
    _mqttBackoffMs = min(_mqttBackoffMs * 2, _mqttBackoffMaxMs);
  }
}

bool HomeAssistantMQTT::connected()
{
  return mqttClient != nullptr && mqttClient->connected();
}

void HomeAssistantMQTT::setNanoAvailable(bool available)
{
  if (!connected()) return;

  const unsigned long now = millis();
  if (_nanoAvailabilityPublished && available == _nanoAvailable &&
      now - _lastNanoAvailabilityPublishMs < AvailabilityHeartbeatIntervalMs) {
    return;
  }

  const char* payload = available ? "{\"state\":\"online\"}" : "{\"state\":\"offline\"}";
  if (!mqttClient->publish(nanoAvailabilityTopic, payload, true)) {
    logMqttPublishFailure("publication disponibilité Nano", strlen(payload), mqttClient);
    return;
  }

  _nanoAvailable = available;
  _nanoAvailabilityPublished = true;
  _lastNanoAvailabilityPublishMs = now;
}

void HomeAssistantMQTT::publishDiscovery(const HaEntityConfig& entity)
{
  if (mqttClient == nullptr || entity.component == nullptr || entity.name == nullptr)
    return;

  char uniqueId[HAMQTT_TOPIC_SIZE];
  snprintf(uniqueId, sizeof(uniqueId), "%s_%s", DeviceName.c_str(), entity.name);
  char commandTopic[HAMQTT_TOPIC_SIZE];
  snprintf(commandTopic, sizeof(commandTopic), "%s%s", commandTopicPrefix, entity.name);
  char valueTemplate[64];
  snprintf(valueTemplate, sizeof(valueTemplate), "{{ value_json.%s }}", entity.name);
  char topic[HAMQTT_TOPIC_SIZE];
  snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config",
           entity.component, DeviceName.c_str(), entity.name);

  JsonDocument document;
  JsonObject availability = document["availability"].to<JsonArray>().add<JsonObject>();
  availability["topic"] = static_cast<const char*>(availabilityTopic);
  availability["value_template"] = "{{ value_json.state }}";
  if (dependsOnNanoTelemetry(entity)) {
    JsonObject nanoAvailability =
        document["availability"].as<JsonArray>().add<JsonObject>();
    nanoAvailability["topic"] = static_cast<const char*>(nanoAvailabilityTopic);
    nanoAvailability["value_template"] = "{{ value_json.state }}";
    document["availability_mode"] = "all";
  }

  JsonObject device = document["device"].to<JsonObject>();
  device["identifiers"].to<JsonArray>().add(DeviceName.c_str());
  device["manufacturer"] = Manufacturer.c_str();
  device["model"] = Model.c_str();
  device["name"] = Name.c_str();
  device["sw_version"] = Version.c_str();

  document["enabled_by_default"] = true;
  if (hasText(entity.category)) document["entity_category"] = entity.category;
  document["unique_id"] = static_cast<const char*>(uniqueId);
  document["name"] = entity.name;
  if (hasText(entity.icon)) document["icon"] = entity.icon;
  if (hasText(entity.unit)) document["unit_of_measurement"] = entity.unit;
  if (hasText(entity.deviceClass)) document["device_class"] = entity.deviceClass;
  if (hasText(entity.stateClass)) document["state_class"] = entity.stateClass;
  if (entity.displayPrecision) document["suggested_display_precision"] = 2;
  if (strcmp(entity.component, "sensor") == 0 ||
      strcmp(entity.component, "binary_sensor") == 0) {
    document["expire_after"] = EntityStateExpireAfterSeconds;
  }

  const bool isBinary = strcmp(entity.component, "binary_sensor") == 0 ||
                        strcmp(entity.component, "switch") == 0;
  if (isBinary) {
    document["payload_off"] = "false";
    document["payload_on"] = "true";
  }
  if (strcmp(entity.component, "number") == 0) {
    document["min"] = entity.min;
    document["max"] = entity.max;
    document["step"] = entity.step;
    document["mode"] = "box";
  }
  if (entity.command) document["command_topic"] = static_cast<const char*>(commandTopic);
  document["state_topic"] = static_cast<const char*>(stateTopic);
  document["value_template"] = static_cast<const char*>(valueTemplate);

  publishJson(mqttClient, topic, document, "publication découverte", true);

  if (entity.command && !mqttClient->subscribe(commandTopic)) {
    logMqttPublishFailure("abonnement commande", strlen(commandTopic), mqttClient);
  }
}

bool HomeAssistantMQTT::setValue(const char* item, const char* value)
{
  if (item == nullptr || value == nullptr) return false;

  for (size_t i = 0; i < valueCount; ++i) {
    if (strcmp(values[i].item, item) == 0) {
      if (strncmp(values[i].value, value, sizeof(values[i].value) - 1) == 0) {
        return false;
      }
      snprintf(values[i].value, sizeof(values[i].value), "%s", value);
      valuesDirty = true;
      return true;
    }
  }

  if (valueCount >= HAMQTT_MAXITEMS) {
    Serial.printf("[MQTT] Trop d'entités, valeur ignorée : %s\n", item);
    return false;
  }
  snprintf(values[valueCount].item, sizeof(values[valueCount].item), "%s", item);
  snprintf(values[valueCount].value, sizeof(values[valueCount].value), "%s", value);
  ++valueCount;
  valuesDirty = true;
  return true;
}

bool HomeAssistantMQTT::sendValues(bool force)
{
  if (mqttClient == nullptr || (!valuesDirty && !force)) return false;

  JsonDocument document;
  for (size_t i = 0; i < valueCount; ++i) {
    // Valeurs envoyées en texte : les entités binaires comparent "true"/"false".
    document[static_cast<const char*>(values[i].item)] =
        static_cast<const char*>(values[i].value);
  }

  const bool sent = publishJson(mqttClient, stateTopic, document, "publication état", false);
  if (sent) {
    valuesDirty = false;
  }
  return sent;
}

void HomeAssistantMQTT::MqttCallback(char* topic, byte* payload, unsigned int length)
{
  const size_t prefixLength = strlen(commandTopicPrefix);
  if (cb_callback == nullptr || strncmp(topic, commandTopicPrefix, prefixLength) != 0)
    return;

  if (length >= MaxCommandPayloadSize) {
    Serial.printf("[MQTT] Commande trop longue ignorée sur %s\n", topic);
    return;
  }

  char text[MaxCommandPayloadSize];
  memcpy(text, payload, length);
  text[length] = '\0';
  cb_callback(topic + prefixLength, text);
}
