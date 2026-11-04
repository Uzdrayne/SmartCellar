#include "SerialLink/SerialLink.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include "Data/ControllerData.h"

namespace {

constexpr unsigned long SerialBaudRate = 9600;
constexpr unsigned long TelemetryTimeoutMs = 6000;
constexpr unsigned long OutputRefreshMs = 1000;
constexpr unsigned long ConfigurationRetryMs = 5000;
constexpr size_t MaxLineLength = 192;
constexpr size_t MaxBytesPerCycle = 128;
constexpr size_t MaxConfigurationMessageLength = 768;

char inputLine[MaxLineLength];
size_t inputLength = 0;
bool discardInputLine = false;
bool telemetryReceived = false;
bool telemetryStale = true;
unsigned long lastTelemetryAt = 0;
unsigned long lastOutputAt = 0;
bool lastOutputStates[5] = {false, false, false, false, false};
bool outputSnapshotSent = false;
uint32_t lastConfigurationRevision = 0;
bool configurationSnapshotSent = false;
bool controllerConfigurationRequested = false;
bool configurationPending = false;
unsigned long lastConfigurationSentAt = 0;

struct ControllerConfiguration {
    float setTemp;
    float setHum;
    bool tempOffsetEnabled;
    float tempOffset;
    bool humOffsetEnabled;
    float humOffset;
    float coldHysHigh;
    float coldHysLow;
    unsigned int coldDelayMinOn;
    unsigned int coldDelayMinOff;
    float heatHysHigh;
    float heatHysLow;
    unsigned int heatDelayMinOn;
    unsigned int heatDelayMinOff;
    float dehumHysHigh;
    float dehumHysLow;
    unsigned int dehumDelayMinOn;
    unsigned int dehumDelayMinOff;
    float humHysHigh;
    float humHysLow;
    unsigned int humDelayMinOn;
    unsigned int humDelayMinOff;
    bool fanEnabled;
    unsigned int fanOnMinutes;
    unsigned int fanOffMinutes;
};

ControllerConfiguration getControllerConfiguration() {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const ControllerConfiguration configuration = {
        DataController.SetTemp,
        DataController.SetHum,
        DataController.TempOffsetEnabled,
        DataController.TempOffset,
        DataController.HumOffsetEnabled,
        DataController.HumOffset,
        DataController.ColdHysHigh,
        DataController.ColdHysLow,
        DataController.ColdDelayMinOn,
        DataController.ColdDelayMinOff,
        DataController.HeatHysHigh,
        DataController.HeatHysLow,
        DataController.HeatDelayMinOn,
        DataController.HeatDelayMinOff,
        DataController.DehumHysHigh,
        DataController.DehumHysLow,
        DataController.DehumDelayMinOn,
        DataController.DehumDelayMinOff,
        DataController.HumHysHigh,
        DataController.HumHysLow,
        DataController.HumDelayMinOn,
        DataController.HumDelayMinOff,
        DataController.FanEnabled,
        DataController.FanOnMinutes,
        DataController.FanOffMinutes
    };
    xSemaphoreGive(DataMutex);
    return configuration;
}

bool sameControllerConfiguration(const ControllerConfiguration& left,
                                 const ControllerConfiguration& right) {
    return left.setTemp == right.setTemp &&
        left.setHum == right.setHum &&
        left.tempOffsetEnabled == right.tempOffsetEnabled &&
        left.tempOffset == right.tempOffset &&
        left.humOffsetEnabled == right.humOffsetEnabled &&
        left.humOffset == right.humOffset &&
        left.coldHysHigh == right.coldHysHigh &&
        left.coldHysLow == right.coldHysLow &&
        left.coldDelayMinOn == right.coldDelayMinOn &&
        left.coldDelayMinOff == right.coldDelayMinOff &&
        left.heatHysHigh == right.heatHysHigh &&
        left.heatHysLow == right.heatHysLow &&
        left.heatDelayMinOn == right.heatDelayMinOn &&
        left.heatDelayMinOff == right.heatDelayMinOff &&
        left.dehumHysHigh == right.dehumHysHigh &&
        left.dehumHysLow == right.dehumHysLow &&
        left.dehumDelayMinOn == right.dehumDelayMinOn &&
        left.dehumDelayMinOff == right.dehumDelayMinOff &&
        left.humHysHigh == right.humHysHigh &&
        left.humHysLow == right.humHysLow &&
        left.humDelayMinOn == right.humDelayMinOn &&
        left.humDelayMinOff == right.humDelayMinOff &&
        left.fanEnabled == right.fanEnabled &&
        left.fanOnMinutes == right.fanOnMinutes &&
        left.fanOffMinutes == right.fanOffMinutes;
}

void sendControllerConfigurationIfChanged(bool force, unsigned long now) {
    const ControllerConfiguration configuration = getControllerConfiguration();
    static ControllerConfiguration lastSentConfiguration;
    const bool changed = !configurationSnapshotSent ||
        !sameControllerConfiguration(configuration, lastSentConfiguration);
    const bool retryDue = configurationPending &&
        now - lastConfigurationSentAt >= ConfigurationRetryMs;

    if (!changed && !force && !retryDue) {
        return;
    }

    const uint32_t revision = changed
        ? lastConfigurationRevision + 1
        : lastConfigurationRevision;
    JsonDocument message;
    message["type"] = "controller_config";
    message["schema_version"] = 1;
    message["revision"] = revision;
    message["set_temp"] = configuration.setTemp;
    message["set_hum"] = configuration.setHum;
    message["temp_offset_enabled"] = configuration.tempOffsetEnabled;
    message["temp_offset"] = configuration.tempOffset;
    message["hum_offset_enabled"] = configuration.humOffsetEnabled;
    message["hum_offset"] = configuration.humOffset;
    message["cold_hys_high"] = configuration.coldHysHigh;
    message["cold_hys_low"] = configuration.coldHysLow;
    message["cold_delay_min_on"] = configuration.coldDelayMinOn;
    message["cold_delay_min_off"] = configuration.coldDelayMinOff;
    message["heat_hys_high"] = configuration.heatHysHigh;
    message["heat_hys_low"] = configuration.heatHysLow;
    message["heat_delay_min_on"] = configuration.heatDelayMinOn;
    message["heat_delay_min_off"] = configuration.heatDelayMinOff;
    message["dehum_hys_high"] = configuration.dehumHysHigh;
    message["dehum_hys_low"] = configuration.dehumHysLow;
    message["dehum_delay_min_on"] = configuration.dehumDelayMinOn;
    message["dehum_delay_min_off"] = configuration.dehumDelayMinOff;
    message["hum_hys_high"] = configuration.humHysHigh;
    message["hum_hys_low"] = configuration.humHysLow;
    message["hum_delay_min_on"] = configuration.humDelayMinOn;
    message["hum_delay_min_off"] = configuration.humDelayMinOff;
    message["fan_enabled"] = configuration.fanEnabled;
    message["fan_on_minutes"] = configuration.fanOnMinutes;
    message["fan_off_minutes"] = configuration.fanOffMinutes;

    char serializedMessage[MaxConfigurationMessageLength];
    const size_t requiredLength = measureJson(message);
    if (requiredLength >= sizeof(serializedMessage)) {
        Serial.println("[UART] Configuration contrôleur trop longue, envoi annulé");
        return;
    }

    const size_t serializedLength =
        serializeJson(message, serializedMessage, sizeof(serializedMessage));
    if (serializedLength != requiredLength) {
        Serial.println("[UART] Échec de sérialisation de la configuration contrôleur");
        return;
    }

    Serial1.write(reinterpret_cast<const uint8_t*>(serializedMessage), serializedLength);
    Serial1.write('\n');

    lastConfigurationRevision = revision;
    lastSentConfiguration = configuration;
    configurationSnapshotSent = true;
    configurationPending = true;
    lastConfigurationSentAt = now;
    Serial.printf("[UART] Configuration contrôleur envoyée (révision %lu)\n",
                  static_cast<unsigned long>(revision));
}

void setSensorDataStale() {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataController.Temperature = NAN;
    DataController.Humidity = NAN;
    DataController.RelayFeedbackValid = false;
    xSemaphoreGive(DataMutex);
}

void processTelemetry(const char *line, unsigned long now) {
    JsonDocument message;
    const DeserializationError error = deserializeJson(message, line);
    if (error) {
        Serial.printf("[UART] JSON invalide : %s\n", error.c_str());
        return;
    }

    const char *type = message["type"] | "";
    if (strcmp(type, "config_request") == 0) {
        controllerConfigurationRequested = true;
        return;
    }

    if (strcmp(type, "config_ack") == 0 || strcmp(type, "config_nack") == 0) {
        const JsonVariantConst revisionValue = message["revision"];
        if (!revisionValue.is<uint32_t>()) {
            Serial.printf("[UART] Réponse de configuration sans révision valide: %s\n", type);
            return;
        }

        const uint32_t revision = revisionValue.as<uint32_t>();
        if (!configurationSnapshotSent || revision != lastConfigurationRevision) {
            Serial.printf("[UART] Réponse de configuration périmée ignorée (révision %lu)\n",
                          static_cast<unsigned long>(revision));
            return;
        }

        if (strcmp(type, "config_ack") == 0) {
            configurationPending = false;
            Serial.printf("[UART] Configuration contrôleur acquittée (révision %lu)\n",
                          static_cast<unsigned long>(revision));
        } else {
            const char *reason = message["reason"] | "raison inconnue";
            configurationPending = strcmp(reason, "persistence_failure") == 0;
            Serial.printf("[UART] Configuration contrôleur rejetée (révision %lu): %s\n",
                          static_cast<unsigned long>(revision), reason);
        }
        return;
    }

    if (strcmp(type, "telemetry") != 0) {
        Serial.printf("[UART] Type de message inattendu : %s\n", type);
        return;
    }

    const JsonVariantConst temperatureValue = message["temperature"];
    const JsonVariantConst humidityValue = message["humidity"];
    if (!temperatureValue.is<float>() || !humidityValue.is<float>()) {
        Serial.println("[UART] Mesures absentes ou non numériques");
        return;
    }

    const float temperature = temperatureValue.as<float>();
    const float humidity = humidityValue.as<float>();
    if (!isfinite(temperature) || temperature < -40.0f || temperature > 80.0f ||
        !isfinite(humidity) || humidity < 0.0f || humidity > 100.0f) {
        Serial.println("[UART] Mesures hors limites DHT22");
        return;
    }

    const JsonArrayConst gpio = message["gpio"].as<JsonArrayConst>();
    bool feedbackValid = gpio.size() == 5;
    bool feedback[5] = {false, false, false, false, false};
    for (size_t i = 0; feedbackValid && i < 5; ++i) {
        if (!gpio[i].is<bool>()) {
            feedbackValid = false;
        } else {
            feedback[i] = gpio[i].as<bool>();
        }
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataController.Temperature = temperature;
    DataController.Humidity = humidity;
    DataController.RelayFeedbackValid = feedbackValid;
    if (feedbackValid) {
        DataController.RelayGpioCold = feedback[0];
        DataController.RelayGpioHeat = feedback[1];
        DataController.RelayGpioDehum = feedback[2];
        DataController.RelayGpioHum = feedback[3];
        DataController.RelayGpioFan = feedback[4];
    }
    xSemaphoreGive(DataMutex);

    telemetryReceived = true;
    telemetryStale = false;
    lastTelemetryAt = now;
}

}

void InitializeSerialLink() {
    Serial1.begin(SerialBaudRate, SERIAL_8N1, CELLAR_LINK_RX_PIN, CELLAR_LINK_TX_PIN);
    Serial.printf("[UART] Serial1 %lu bauds, RX GPIO%d, TX GPIO%d\n",
                  SerialBaudRate, CELLAR_LINK_RX_PIN, CELLAR_LINK_TX_PIN);
}

void ProcessSerialLink(const unsigned long now) {
    size_t bytesProcessed = 0;
    while (Serial1.available() > 0 && bytesProcessed < MaxBytesPerCycle) {
        const char value = static_cast<char>(Serial1.read());
        ++bytesProcessed;

        if (value == '\n') {
            if (!discardInputLine && inputLength > 0) {
                inputLine[inputLength] = '\0';
                processTelemetry(inputLine, now);
            } else if (discardInputLine) {
                Serial.println("[UART] Ligne trop longue ignorée");
            }

            inputLength = 0;
            discardInputLine = false;
        } else if (value != '\r' && !discardInputLine) {
            if (inputLength < sizeof(inputLine) - 1) {
                inputLine[inputLength++] = value;
            } else {
                inputLength = 0;
                discardInputLine = true;
            }
        }
    }

    if (telemetryReceived && !telemetryStale &&
        now - lastTelemetryAt > TelemetryTimeoutMs) {
        setSensorDataStale();
        telemetryStale = true;
        Serial.println("[UART] Mesures périmées, régulation en sécurité");
    }

    const bool forceConfigurationSend = controllerConfigurationRequested;
    controllerConfigurationRequested = false;
    sendControllerConfigurationIfChanged(forceConfigurationSend, now);
}

void SendRelayOutputs() {
    const unsigned long now = millis();
    bool states[5];

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    states[0] = DataController.ColdState;
    states[1] = DataController.HeatState;
    states[2] = DataController.DehumState;
    states[3] = DataController.HumState;
    states[4] = DataController.FanState;
    xSemaphoreGive(DataMutex);

    bool changed = !outputSnapshotSent;
    for (size_t i = 0; i < 5; ++i) {
        changed = changed || states[i] != lastOutputStates[i];
    }
    if (!changed && now - lastOutputAt < OutputRefreshMs) {
        return;
    }

    JsonDocument message;
    message["type"] = "outputs";
    JsonArray gpio = message["gpio"].to<JsonArray>();
    for (const bool state : states) {
        gpio.add(state);
    }

    serializeJson(message, Serial1);
    Serial1.write('\n');

    memcpy(lastOutputStates, states, sizeof(states));
    outputSnapshotSent = true;
    lastOutputAt = now;
}
