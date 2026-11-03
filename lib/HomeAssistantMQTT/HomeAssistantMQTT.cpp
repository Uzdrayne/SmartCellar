#include "HomeAssistantMQTT.h"

//#define DEBUG 
#define CFG_ON_SERIAL

HomeAssistantMQTT::HomeAssistantMQTT()
{
  for (int i = 0; i < HAMQTT_MAXITEMS; i++)
    values[i] = nullptr;
}

void HomeAssistantMQTT::setCallback(CallbackFunction f)
{
  cb_callback = f;
}

void HomeAssistantMQTT::begin(WiFiClient* wifiClient, const char* server, const uint16_t port)
{
  // Build availability topic: <Manufacturer>/<DeviceName>/state
  {
    const size_t ln = Manufacturer.length() + 1 + DeviceName.length() + 1 + 5 + 1; // man + '/' + dev + '/' + "state" + '\0'
    MqttStateTopic = new char[ln];
    strcpy(MqttStateTopic, Manufacturer.c_str());
    strcat(MqttStateTopic, "/");
    strcat(MqttStateTopic, DeviceName.c_str());
    strcat(MqttStateTopic, "/state");
  }

  // Build state topic: <Manufacturer>/<DeviceName>
  {
    const size_t ln = Manufacturer.length() + 1 + DeviceName.length() + 1; // man + '/' + dev + '\0'
    StateTopic = new char[ln];
    strcpy(StateTopic, Manufacturer.c_str());
    strcat(StateTopic, "/");
    strcat(StateTopic, DeviceName.c_str());
  }

#ifdef DEBUG
  Serial.print("MqttStateTopic: \""); Serial.print(MqttStateTopic); Serial.println("\"");
  Serial.print("StateTopic:     \""); Serial.print(StateTopic);     Serial.println("\"");
#endif

  mqttClient = new PubSubClient(*wifiClient);
  mqttClient->setBufferSize(2048); // allow larger HA discovery payloads
  mqttClient->setServer(server, port);
  mqttClient->setCallback(std::bind(&HomeAssistantMQTT::MqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  mqttClient->setKeepAlive(5);
}

void HomeAssistantMQTT::loop()
{
  if (mqttClient == nullptr) return;

  if (!mqttClient->connected())
    connect();
  
  mqttClient->loop();
}


void HomeAssistantMQTT::connect()
{
  if (mqttClient == nullptr) return;

  // Wi-Fi non connecté → ne tente pas MQTT
    if (WiFi.status() != WL_CONNECTED) {
#ifdef CFG_ON_SERIAL
      Serial.println("WiFi pas encore connecté, on attend avant MQTT...");
#endif
      return;
    }


  // Si déjà connecté : rien à faire
  if (mqttClient->connected()) return;

  unsigned long now = millis();
  if (now < _nextMqttAttemptMs) return;   // on attend l’échéance sans bloquer

#ifdef CFG_ON_SERIAL
  Serial.println("Attempting MQTT connection...");
#endif

  String mqttClientId = DeviceName;
  bool ok = mqttClient->connect(
      mqttClientId.c_str(),
      MqttUser.c_str(), MqttPassword.c_str(),
      MqttStateTopic, 1, true, "{\"state\":\"offline\"}"
  );

  if (ok) {
    mqttClient->publish(MqttStateTopic, "{\"state\":\"online\"}", true);
#ifdef CFG_ON_SERIAL
    Serial.println("MQTT connected");
#endif
    // reset backoff après succès
    _mqttBackoffMs = 1000;
    _nextMqttAttemptMs = now; // prêt pour la suite
  } else {
#ifdef CFG_ON_SERIAL
    Serial.print("MQTT failed, rc=");
    Serial.print(mqttClient->state());
    Serial.print(" - retry in ");
    Serial.print(_mqttBackoffMs);
    Serial.println(" ms");
#endif
    // planifie une nouvelle tentative plus tard (exponentiel limité)
    _nextMqttAttemptMs = now + _mqttBackoffMs;
    _mqttBackoffMs = min(_mqttBackoffMs * 2, _mqttBackoffMaxMs);
  }
}


bool HomeAssistantMQTT::connected()
{
  return mqttClient != nullptr && mqttClient->connected();
}

void HomeAssistantMQTT::publishConfigSensor(String deviceClass, String stateClass, String name, String icon, String unit, String startupValue)
{
  String complement = ",\"suggested_display_precision\":2";

  publishConfig("sensor", "", deviceClass, stateClass, name, icon, unit, false, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigBinarySensor(String deviceClass, String name, String icon, String payloadOff, String payloadOn, String startupValue)
{
  String complement = ",\"payload_off\":\"" + payloadOff + "\",\"payload_on\":\"" + payloadOn + "\"";
  publishConfig("binary_sensor", "", deviceClass, "", name, icon, "", false, true, "", complement, startupValue);
}


void HomeAssistantMQTT::publishConfigNumber(String category, String name, String icon, String unit, String min, String max, String startupValue, String step)
{
  //String complement = ",\"min\":" + min + ",\"max\":" + max + ",\"suggested_display_precision \":" + "2"  ;
  String complement =
    ",\"min\":" + min +
    ",\"max\":" + max +
    ",\"step\":" + step +
    ",\"mode\":\"box\"" +
    ",\"suggested_display_precision\":2";

  publishConfig("number", category, "", "", name, icon, unit, true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigButton(String category, String name, String icon, String commandTopicName, String payload)
{
  String complement = ",\"payload_press\":\"" + payload + "\"";
  publishConfig("button", category, "", "", name, icon, "", true, false, commandTopicName, complement, "");
}

void HomeAssistantMQTT::publishConfigSelect(String category, String name, String icon, String options[], unsigned short optionsCount, String startupValue)
{
  String complement = ",\"options\":[\"";
  for (int i = 0; i < optionsCount; i++)
  {
    complement += (i > 0 ? "\",\"" : "") + options[i];
  }
  complement += "\"]";
  publishConfig("select", category, "", "", name, icon, "", true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigSwitch(String category, String name, String icon, String startupValue)
{
  String complement = ",\"payload_off\":\"false\",\"payload_on\":\"true\"";
  publishConfig("switch", category, "", "", name, icon, "", true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfig(const char* type, String category, String deviceClass, String stateClass, String name, String icon, String unit, bool commandTopic, bool stateTopic, String commandTopicName, String complement, String startupValue)
{
  String nameForTopic = (name.length() > 0 ? name : deviceClass);
  nameForTopic.replace(" ", "_");
  String COMMAND_TOPIC = Manufacturer + "/" + DeviceName + "/set/" + (commandTopicName.length() > 0 ? commandTopicName : nameForTopic);

  String topic = "homeassistant/" + String(type) + "/" + DeviceName + "/" + nameForTopic + "/config";
  String data = "{\"availability\":[{\"topic\":\"" + String(MqttStateTopic) + "\",\"value_template\":\"{{ value_json.state }}\"}]"
      + ",\"device\":{\"identifiers\":[\"" + DeviceName + "\"],\"manufacturer\":\"" + Manufacturer + "\",\"model\":\"" + Model + "\",\"name\":\"" + Name + "\",\"sw_version\":\"" + Version + "\"}"
      + ",\"enabled_by_default\":true"
      + (category.length() > 0 ? ",\"entity_category\":\"" + category + "\"" : "")
      + ",\"unique_id\":\"" + DeviceName + "_" + nameForTopic + "\""
      + (name.length() > 0 ? ",\"name\":\"" + name + "\"" : "")
      + (icon.length() > 0 ? ",\"icon\":\"" + icon + "\"" : "")
      + (unit.length() > 0 ? ",\"unit_of_measurement\":\"" + unit + "\"" : "")
      + (deviceClass.length() > 0 ? ",\"device_class\":\"" + deviceClass + "\"" : "")
      + (stateClass.length() > 0 ? ",\"state_class\":\"" + stateClass + "\"" : "")
      + complement
      + (commandTopic ? ",\"command_topic\":\"" + COMMAND_TOPIC + "\"" : "")
      + (stateTopic ? ",\"state_topic\":\"" + String(StateTopic) + "\"" : "")
      + (stateTopic ? ",\"value_template\":\"{{ value_json." + nameForTopic + " }}\"" : "")
      + "}";
/*
#ifdef CFG_ON_SERIAL
  Serial.print("  - ");
  Serial.println(topic.c_str());
  Serial.print("    ");
  Serial.println(data.c_str());
#endif
*/

  mqttClient->publish(topic.c_str(), data.c_str(), true);

  if (commandTopic)
    mqttClient->subscribe(COMMAND_TOPIC.c_str());

  if (stateTopic)
    setValue(nameForTopic, startupValue);
}

void HomeAssistantMQTT::setValue(String item, String value)
{
  bool bFound = false;
  int i = 0;
  while (i < HAMQTT_MAXITEMS && !bFound)
  {
    if (values[i] != nullptr)
    {
      if (strcmp(values[i]->item, item.c_str()) == 0)
      {
        // Reallocate value buffer if needed to avoid overflow
        size_t needed = value.length() + 1;
        size_t current = strlen(values[i]->value) + 1;
        if (needed > current) {
          delete[] values[i]->value;
          values[i]->value = new char[needed];
        }
        strcpy(values[i]->value, value.c_str());
        bFound = true;
      }
    }
    else
    {
      // Create new entry sized to content
      ItemValue* iv = new ItemValue;
      iv->item  = new char[item.length() + 1];
      iv->value = new char[value.length() + 1];
      strcpy(iv->item,  item.c_str());
      strcpy(iv->value, value.c_str());
      values[i] = iv;
      bFound = true;
    }
    i++;
  }
}

String HomeAssistantMQTT::getValue(String item)
{
  for (int i = 0; i < HAMQTT_MAXITEMS; i++)
  {
    if (values[i] != nullptr)
    {
      if (strcmp(values[i]->item, item.c_str()) == 0)
        return String(values[i]->value);
    }
  }
  return String("");
}

void HomeAssistantMQTT::readValues()
{
#ifdef DEBUG
  Serial.print("readValues: topic \"");
  Serial.print(StateTopic);
  Serial.println("\"");
#endif
  mqttClient->subscribe(StateTopic);
}

void HomeAssistantMQTT::sendValues()
{
  String str = "{";
  bool first = true;
  for (int i = 0; i < HAMQTT_MAXITEMS; i++)
  {
    if (values[i] != nullptr)
    {
      if (!first) str += ",";
      str += "\"";
      str += values[i]->item;
      str += "\":\"";
      str += values[i]->value;
      str += "\"";
      first = false;
    }
  }
  str += "}";
  
  mqttClient->publish(StateTopic, str.c_str(), true);
}

void HomeAssistantMQTT::MqttCallback(char* topic, byte* payload, unsigned int length)
{
#ifdef DEBUG
  Serial.print("Message arrived on topic: '");
  Serial.print(topic);
  Serial.print("' with payload: ");
#endif

  // Copy payload into String (no VLA)
  String payloadStr;
  payloadStr.reserve(length);
  for (unsigned int i = 0; i < length; i++)
  {
    payloadStr += (char)payload[i];
  }

#ifdef DEBUG
  Serial.println(payloadStr);
#endif

  if (strcmp(StateTopic, topic) == 0)
  {
    // Received message on device state topic, read the values and unsubscribe from the topic
   // DynamicJsonDocument doc(length + 64);

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payloadStr);
    if (error)
    {
#ifdef DEBUG
      Serial.print("Error reading old values, deserializeJson() returned: ");
      Serial.println(error.c_str());
#endif
    }
    else
    {
      for (JsonPair kv : doc.as<JsonObject>())
      {
#ifdef DEBUG
        Serial.print("Key: \"");
        Serial.print(kv.key().c_str());
        Serial.print("\", Value: \"");
        Serial.print(kv.value().as<const char*>());
        Serial.println("\"");
#endif
        setValue(String(kv.key().c_str()), String(kv.value().as<const char*>())) ;
        if (cb_callback != NULL)
          cb_callback(String(kv.key().c_str()), String(kv.value().as<const char*>()), true);
      }
      sendValues();
    }
    
    mqttClient->unsubscribe(StateTopic);
  }
  else
  {
    String COMMAND_TOPIC = Manufacturer + "/" + DeviceName + "/set/";
    size_t baseLen = COMMAND_TOPIC.length();
    if (strncmp(topic, COMMAND_TOPIC.c_str(), baseLen) == 0)
    {
      String item = String(topic).substring(baseLen);
      if (cb_callback != NULL)
        cb_callback(item, payloadStr, false);
    }
  }
}
