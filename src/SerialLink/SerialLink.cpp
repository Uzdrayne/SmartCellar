#include "SerialLink/SerialLink.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include "Data/ControllerData.h"

namespace {

constexpr unsigned long SerialBaudRate = 9600;
constexpr unsigned long TelemetryTimeoutMs = 6000;
constexpr unsigned long OutputRefreshMs = 1000;
constexpr size_t MaxLineLength = 192;
constexpr size_t MaxBytesPerCycle = 128;

char inputLine[MaxLineLength];
size_t inputLength = 0;
bool discardInputLine = false;
bool telemetryReceived = false;
bool telemetryStale = true;
unsigned long lastTelemetryAt = 0;
unsigned long lastOutputAt = 0;
bool lastOutputStates[5] = {false, false, false, false, false};
bool outputSnapshotSent = false;

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
