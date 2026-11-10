#include "ServerWeb/WsCommands.h"

#include "Controller/ConfigurationLimits.h"
#include "Controller/ControllerValidation.h"
#include "Data/ControllerData.h"

namespace wsapi {

namespace {

bool setRelaySettings(const WsCommand& cmd) {
    String kind;
    float hysHigh;
    float hysLow;
    unsigned int delayMinOn;
    unsigned int delayMinOff;
    if (!readString(cmd.payload, "kind", 16, kind) ||
        !readFiniteFloat(cmd.payload, "hysHigh", hysHigh) ||
        !readFiniteFloat(cmd.payload, "hysLow", hysLow) ||
        !readBoundedUnsigned(cmd.payload, "delayMinOn", MinRelayDelaySeconds,
                             MaxRelayDelaySeconds, delayMinOn) ||
        !readBoundedUnsigned(cmd.payload, "delayMinOff", MinRelayDelaySeconds,
                             MaxRelayDelaySeconds, delayMinOff)) {
        sendError(cmd, "invalid_parameters",
                  "Paramètres de relais manquants ou invalides.", true);
        return false;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    StrctDataController candidate = DataController;
    xSemaphoreGive(DataMutex);

    if (kind == "cold") {
        candidate.ColdHysHigh = hysHigh;
        candidate.ColdHysLow = hysLow;
        candidate.ColdDelayMinOn = delayMinOn;
        candidate.ColdDelayMinOff = delayMinOff;
    } else if (kind == "heat") {
        candidate.HeatHysHigh = hysHigh;
        candidate.HeatHysLow = hysLow;
        candidate.HeatDelayMinOn = delayMinOn;
        candidate.HeatDelayMinOff = delayMinOff;
    } else if (kind == "dehum") {
        candidate.DehumHysHigh = hysHigh;
        candidate.DehumHysLow = hysLow;
        candidate.DehumDelayMinOn = delayMinOn;
        candidate.DehumDelayMinOff = delayMinOff;
    } else if (kind == "hum") {
        candidate.HumHysHigh = hysHigh;
        candidate.HumHysLow = hysLow;
        candidate.HumDelayMinOn = delayMinOn;
        candidate.HumDelayMinOff = delayMinOff;
    } else {
        sendError(cmd, "unknown_relay",
                  "Type de relais inconnu.", true);
        return false;
    }

    const bool temperatureRelay = kind == "cold" || kind == "heat";
    const bool valid = temperatureRelay
        ? IsValidTemperatureRelaySettings(candidate)
        : IsValidHumidityRelaySettings(candidate);
    if (!valid) {
        sendError(cmd, "invalid_settings",
                  "Hystérésis incohérentes ou hors limites.", true);
        return false;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    if (kind == "cold") {
        DataController.ColdHysHigh = hysHigh;
        DataController.ColdHysLow = hysLow;
        DataController.ColdDelayMinOn = delayMinOn;
        DataController.ColdDelayMinOff = delayMinOff;
    } else if (kind == "heat") {
        DataController.HeatHysHigh = hysHigh;
        DataController.HeatHysLow = hysLow;
        DataController.HeatDelayMinOn = delayMinOn;
        DataController.HeatDelayMinOff = delayMinOff;
    } else if (kind == "dehum") {
        DataController.DehumHysHigh = hysHigh;
        DataController.DehumHysLow = hysLow;
        DataController.DehumDelayMinOn = delayMinOn;
        DataController.DehumDelayMinOff = delayMinOff;
    } else {
        DataController.HumHysHigh = hysHigh;
        DataController.HumHysLow = hysLow;
        DataController.HumDelayMinOn = delayMinOn;
        DataController.HumDelayMinOff = delayMinOff;
    }
    xSemaphoreGive(DataMutex);

    bool persisted = false;
    if (kind == "cold") persisted = Save_ColdHardware();
    else if (kind == "heat") persisted = Save_HeatHardware();
    else if (kind == "dehum") persisted = Save_DehumHardware();
    else persisted = Save_HumHardware();

    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted), true);
    return persisted;
}

void handleSetTemperature(const WsCommand& cmd) {
    float floatValue;
    if (!readFiniteFloat(cmd.payload, "value", floatValue)) {
        sendError(cmd, "invalid_parameters",
                  "La valeur numérique est absente ou invalide.", true);
        return;
    }
    bool valid;
    bool persisted;
    if (cmd.action == "setTemperature") {
        valid = IsValidTemperatureSetpoint(floatValue);
        if (!valid) {
            sendError(cmd, "out_of_range",
                      "Consigne de température hors limites.", true);
            return;
        }
        xSemaphoreTake(DataMutex, portMAX_DELAY);
        DataController.SetTemp = floatValue;
        xSemaphoreGive(DataMutex);
        persisted = Save_SetTemp();
    } else {
        valid = IsValidHumiditySetpoint(floatValue);
        if (!valid) {
            sendError(cmd, "out_of_range",
                      "Consigne d'humidité hors limites.", true);
            return;
        }
        xSemaphoreTake(DataMutex, portMAX_DELAY);
        DataController.SetHum = floatValue;
        xSemaphoreGive(DataMutex);
        persisted = Save_SetHum();
    }
    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted), true);
    return;
}

void handleGetLimits(const WsCommand& cmd) {
    JsonDocument result;
    FillConfigurationLimits(result.to<JsonObject>());
    sendReadResult(cmd, result);
    return;
}

void handleSaveSensorCalibration(const WsCommand& cmd) {
    bool tempEnabled;
    bool humEnabled;
    float tempOffset;
    float humOffset;
    if (!readBoolean(cmd.payload, "tempEnabled", tempEnabled) ||
        !readFiniteFloat(cmd.payload, "tempOffset", tempOffset) ||
        !readBoolean(cmd.payload, "humEnabled", humEnabled) ||
        !readFiniteFloat(cmd.payload, "humOffset", humOffset) ||
        !IsValidTemperatureOffset(tempOffset) ||
        !IsValidHumidityOffset(humOffset)) {
        sendError(cmd, "invalid_parameters",
                  "Calibration invalide ou hors limites.", true);
        return;
    }
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataController.TempOffsetEnabled = tempEnabled;
    DataController.TempOffset = tempOffset;
    DataController.HumOffsetEnabled = humEnabled;
    DataController.HumOffset = humOffset;
    xSemaphoreGive(DataMutex);
    const bool persisted = Save_SensorOffset();
    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted), true);
    return;
}

void handleSetRelaySettings(const WsCommand& cmd) {
    setRelaySettings(cmd);
    return;
}

void handleSaveFanSettings(const WsCommand& cmd) {
    bool enabled;
    unsigned int onMinutes;
    unsigned int offMinutes;
    if (!readBoolean(cmd.payload, "enabled", enabled) ||
        !readBoundedUnsigned(cmd.payload, "onMinutes", FanDurationMinMinutes,
                             FanDurationMaxMinutes, onMinutes) ||
        !readBoundedUnsigned(cmd.payload, "offMinutes", FanDurationMinMinutes,
                             FanDurationMaxMinutes, offMinutes)) {
        sendError(cmd, "invalid_parameters",
                  "Paramètres de ventilation manquants ou hors limites.", true);
        return;
    }
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataController.FanEnabled = enabled;
    DataController.FanOnMinutes = onMinutes;
    DataController.FanOffMinutes = offMinutes;
    xSemaphoreGive(DataMutex);
    const bool persisted = Save_FanSettings();
    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted), true);
    return;
}

}

const WsAction SettingsActions[] = {
    {"setTemperature", handleSetTemperature},
    {"setHumidity", handleSetTemperature},
    {"getLimits", handleGetLimits},
    {"saveSensorCalibration", handleSaveSensorCalibration},
    {"setRelaySettings", handleSetRelaySettings},
    {"saveFanSettings", handleSaveFanSettings},
};
const size_t SettingsActionCount =
    sizeof(SettingsActions) / sizeof(SettingsActions[0]);

}
