#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <stdlib.h>

#include <HomeAssistantMQTT.h>

#include "Alarm/SafetyAlarm.h"
#include "Controller/ConfigurationLimits.h"
#include "Data/ControllerData.h"
#include "Controller/ControllerValidation.h"
#include "WifiAndMqtt/my_MQTT.h"

namespace {

// Lecture des valeurs toutes les 2 s; l'état n'est publié que s'il a changé,
// ou toutes les 5 minutes pour que Home Assistant le sache vivant.
constexpr unsigned long ValueCheckIntervalMs = 2000;
constexpr unsigned long ForcedPublishIntervalMs = 300000;

TaskHandle_t mqttTaskHandle = nullptr;
bool clientInitialized = false;
bool discoveryPublished = false;
unsigned long lastValueCheck = 0;
unsigned long lastPublishAt = 0;

// Instantané commun à toutes les entités pour un cycle de publication.
struct MqttSnapshot {
    StrctDataController data;
    CorrectedMeasurements measured;
    SafetyAlarmStatus alarm;
};

using ValueFormatter = void (*)(const MqttSnapshot&, char*, size_t);

struct MqttEntity {
    HaEntityConfig config;
    ValueFormatter format;
};

void formatFloat(float value, int decimals, char* out, size_t size) {
    if (isfinite(value)) {
        snprintf(out, size, "%.*f", decimals, static_cast<double>(value));
    } else {
        snprintf(out, size, "unknown");
    }
}

void formatBool(bool value, char* out, size_t size) {
    snprintf(out, size, "%s", value ? "true" : "false");
}

void formatUnsigned(unsigned long value, char* out, size_t size) {
    snprintf(out, size, "%lu", value);
}

// Table unique : ajouter une entité Home Assistant = ajouter une ligne.
// Les noms (clés d'état, unique_id, topics) sont ceux déjà connus de HA.
const MqttEntity Entities[] = {
    {{"sensor", "Temperature", nullptr, "temperature", "measurement", nullptr, "°C", false, true},
     [](const MqttSnapshot& s, char* o, size_t n) { formatFloat(s.measured.Temperature, 2, o, n); }},
    {{"sensor", "Humidity", nullptr, "humidity", "measurement", nullptr, "%", false, true},
     [](const MqttSnapshot& s, char* o, size_t n) { formatFloat(s.measured.Humidity, 2, o, n); }},
    {{"binary_sensor", "DeviceAlert", nullptr, "problem", nullptr, "mdi:alert", nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.alarm.AlertActive, o, n); }},
    {{"sensor", "AlertStatus", nullptr, nullptr, nullptr, "mdi:alert-circle-outline", nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) {
         snprintf(o, n, "%s", GetSafetyAlarmReasonText(s.alarm).c_str());
     }},
    {{"sensor", "LastValidTemperature", nullptr, "temperature", "measurement", "mdi:thermometer-check", "°C", false, true},
     [](const MqttSnapshot& s, char* o, size_t n) {
         formatFloat(s.alarm.HasLastMeasurement ? s.alarm.LastValidTemperatureC : NAN, 1, o, n);
     }},
    {{"sensor", "LastMeasurementAge", nullptr, nullptr, nullptr, "mdi:clock-outline", "s", false, true},
     [](const MqttSnapshot& s, char* o, size_t n) {
         if (s.alarm.HasLastMeasurement) formatUnsigned(s.alarm.LastMeasurementAgeSeconds, o, n);
         else snprintf(o, n, "unknown");
     }},
    {{"number", "ConsigneTemperature", "config", nullptr, nullptr, "mdi:Thermometer-Lines", "°C", true, true,
      MinTemperatureSetpointC, MaxTemperatureSetpointC, 0.1f},
     [](const MqttSnapshot& s, char* o, size_t n) { formatFloat(s.data.SetTemp, 2, o, n); }},
    {{"number", "ConsigneHumidity", "config", nullptr, nullptr, "mdi:water-percent", "%", true, true,
      MinHumiditySetpointPercent, MaxHumiditySetpointPercent, 0.1f},
     [](const MqttSnapshot& s, char* o, size_t n) { formatFloat(s.data.SetHum, 2, o, n); }},
    {{"binary_sensor", "Cold_Relay", nullptr, nullptr, nullptr, nullptr, nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.ColdState, o, n); }},
    {{"binary_sensor", "Heat_Relay", nullptr, nullptr, nullptr, nullptr, nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.HeatState, o, n); }},
    {{"binary_sensor", "Dehum_Relay", nullptr, nullptr, nullptr, nullptr, nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.DehumState, o, n); }},
    {{"binary_sensor", "Hum_Relay", nullptr, nullptr, nullptr, nullptr, nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.HumState, o, n); }},
    {{"binary_sensor", "Ventilation_Relay", nullptr, nullptr, nullptr, "mdi:fan", nullptr, false, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.FanState, o, n); }},
    {{"switch", "Ventilation_Enabled", "config", nullptr, nullptr, "mdi:fan-auto", nullptr, true, false},
     [](const MqttSnapshot& s, char* o, size_t n) { formatBool(s.data.FanEnabled, o, n); }},
    {{"number", "Ventilation_On_Minutes", "config", nullptr, nullptr, "mdi:timer-play", "min", true, true,
      static_cast<float>(FanDurationMinMinutes), static_cast<float>(FanDurationMaxMinutes), 1.0f},
     [](const MqttSnapshot& s, char* o, size_t n) { formatUnsigned(s.data.FanOnMinutes, o, n); }},
    {{"number", "Ventilation_Off_Minutes", "config", nullptr, nullptr, "mdi:timer-pause", "min", true, true,
      static_cast<float>(FanDurationMinMinutes), static_cast<float>(FanDurationMaxMinutes), 1.0f},
     [](const MqttSnapshot& s, char* o, size_t n) { formatUnsigned(s.data.FanOffMinutes, o, n); }},
};

MqttSnapshot takeSnapshot() {
    MqttSnapshot snapshot;
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    snapshot.data = DataController;
    xSemaphoreGive(DataMutex);
    snapshot.measured = GetCorrectedMeasurements(snapshot.data);
    snapshot.alarm = GetSafetyAlarmStatus();
    return snapshot;
}

void updateValues(bool forcePublish) {
    const MqttSnapshot snapshot = takeSnapshot();
    char value[HAMQTT_ITEM_VALUE_SIZE];
    for (const MqttEntity& entity : Entities) {
        entity.format(snapshot, value, sizeof(value));
        mqtt.setValue(entity.config.name, value);
    }
    if (mqtt.sendValues(forcePublish)) {
        lastPublishAt = millis();
    }
}

void publishDiscovery() {
    for (const MqttEntity& entity : Entities) {
        mqtt.publishDiscovery(entity.config);
    }
}

bool parseFanMinutes(const char* text, unsigned int& value) {
    return ParseBoundedUnsigned(String(text), FanDurationMinMinutes,
                                FanDurationMaxMinutes, value);
}

// Commandes reçues de Home Assistant (exécutées dans la tâche MQTT).
void handleCommand(const char* item, const char* payload) {
    if (strcmp(item, "ConsigneTemperature") == 0 ||
        strcmp(item, "ConsigneHumidity") == 0) {
        const bool temperature = strcmp(item, "ConsigneTemperature") == 0;
        float value;
        if (!ParseFiniteFloat(String(payload), value) ||
            !(temperature ? IsValidTemperatureSetpoint(value)
                          : IsValidHumiditySetpoint(value))) {
            Serial.printf("MQTT : consigne invalide pour %s : %s\n", item, payload);
            return;
        }
        xSemaphoreTake(DataMutex, portMAX_DELAY);
        float& setpoint = temperature ? DataController.SetTemp : DataController.SetHum;
        const bool changed = setpoint != value;
        setpoint = value;
        xSemaphoreGive(DataMutex);
        if (changed) {
            // Persisté comme depuis l'écran ou le Web.
            temperature ? Save_SetTemp() : Save_SetHum();
            Serial.printf("MQTT : %s = %.2f\n", item, static_cast<double>(value));
        }
        return;
    }

    if (strcmp(item, "Ventilation_Enabled") == 0) {
        bool value;
        if (strcmp(payload, "true") == 0 || strcmp(payload, "ON") == 0) {
            value = true;
        } else if (strcmp(payload, "false") == 0 || strcmp(payload, "OFF") == 0) {
            value = false;
        } else {
            Serial.printf("MQTT : valeur Ventilation_Enabled invalide : %s\n", payload);
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
        return;
    }

    if (strcmp(item, "Ventilation_On_Minutes") == 0 ||
        strcmp(item, "Ventilation_Off_Minutes") == 0) {
        unsigned int value;
        if (!parseFanMinutes(payload, value)) {
            Serial.printf("MQTT : duree de ventilation invalide pour %s : %s\n", item, payload);
            return;
        }
        xSemaphoreTake(DataMutex, portMAX_DELAY);
        unsigned int& setting = strcmp(item, "Ventilation_On_Minutes") == 0
            ? DataController.FanOnMinutes
            : DataController.FanOffMinutes;
        const bool changed = setting != value;
        setting = value;
        xSemaphoreGive(DataMutex);
        if (changed) {
            Save_FanSettings();
            Serial.printf("MQTT : %s = %u min\n", item, value);
        }
        return;
    }

    Serial.printf("MQTT : commande inconnue : %s\n", item);
}

void mqttTask(void*) {
    for (;;) {
        if (!Get_WifiScanning()) {
            MyMqttLoop();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void startMqttTask() {
    if (mqttTaskHandle != nullptr) {
        return;
    }

    if (xTaskCreate(mqttTask, "mqtt", 8192, nullptr, 1, &mqttTaskHandle) != pdPASS) {
        mqttTaskHandle = nullptr;
        Serial.println("[MQTT] Impossible de démarrer la tâche réseau MQTT.");
    }
}

}

void ConnectMqtt() {
    if (!clientInitialized) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macText[13];
        snprintf(macText, sizeof(macText), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        xSemaphoreTake(DataMutex, portMAX_DELAY);
        mqtt.MqttUser = DataMqtt.MqttUser;
        mqtt.MqttPassword = DataMqtt.MqttPassword;
        mqtt.Manufacturer = DataMqtt.MqttManu;
        mqtt.Model = DataMqtt.MqttModel;
        mqtt.Version = DataMqtt.MqttVersion;
        mqtt.Name = DataMqtt.MqttDeviceName;
        mqtt.DeviceName = DataMqtt.MqttManu + "_" + DataMqtt.MqttModel + "_" + macText;
        const String server = DataMqtt.MqttServer;
        const uint16_t port = DataMqtt.MqttPort;
        xSemaphoreGive(DataMutex);

        mqtt.setCallback(handleCommand);
        wifiClient.setTimeout(2);
        mqtt.begin(&wifiClient, server.c_str(), port);
        clientInitialized = true;
    }

    // Le client est conservé entre les reconnexions Wi-Fi.
    startMqttTask();
}

// Connexion et publication MQTT, hors de la boucle de régulation.
void MyMqttLoop() {
    const unsigned long now = millis();

    Set_MqttConnected(mqtt.connected());
    if (!mqtt.connected()) {
        discoveryPublished = false;
    }

    mqtt.loop();
    if (!mqtt.connected()) {
        return;
    }

    if (!discoveryPublished) {
        publishDiscovery();
        updateValues(true);
        discoveryPublished = true;
        lastValueCheck = now;
        return;
    }

    if (now - lastValueCheck >= ValueCheckIntervalMs) {
        lastValueCheck = now;
        updateValues(now - lastPublishAt >= ForcedPublishIntervalMs);
    }
}
