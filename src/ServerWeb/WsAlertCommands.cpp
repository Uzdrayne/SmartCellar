#include "ServerWeb/WsCommands.h"

#include "Alarm/AlertLog.h"
#include "Alarm/SafetyAlarm.h"
#include "Data/ControllerData.h"

namespace wsapi {

namespace {

void handleGetAlertLog(const WsCommand& cmd) {
    AlertLogEntry entries[AlertLogCapacity];
    const size_t count = GetAlertLogEntries(entries, AlertLogCapacity);
    JsonDocument result;
    result["revision"] = GetAlertLogRevision();
    result["uptimeMs"] = millis();
    JsonArray list = result["entries"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject entry = list.add<JsonObject>();
        entry["type"] = GetAlertTypeCode(entries[i].Type);
        entry["active"] = entries[i].Active;
        entry["occurrences"] = entries[i].Occurrences;
        entry["raisedAtMs"] = entries[i].RaisedAtMs;
        entry["lastChangeMs"] = entries[i].LastChangeMs;
        if (entries[i].Type == AlertType::AbnormalRestart) {
            entry["resetReason"] = entries[i].Detail;
        }
    }
    sendReadResult(cmd, result);
    return;
}

void handleClearAlertLog(const WsCommand& cmd) {
    JsonDocument result;
    result["removed"] = ClearEndedAlertLogEntries();
    result["revision"] = GetAlertLogRevision();
    sendReadResult(cmd, result);
    return;
}

void handleGetAlarmSettings(const WsCommand& cmd) {
    const SafetyAlarmConfiguration configuration =
        GetSafetyAlarmConfiguration();
    const SafetyAlarmStatus status = GetSafetyAlarmStatus();
    JsonDocument result;
    result["thresholdC"] = configuration.HighTemperatureThresholdC;
    result["thresholdMinC"] = SafetyAlarmThresholdMinC;
    result["thresholdMaxC"] = SafetyAlarmThresholdMaxC;
    result["triggerDelaySeconds"] = SafetyAlarmTriggerDelaySeconds;
    result["recoveryDelaySeconds"] = SafetyAlarmRecoveryDelaySeconds;
    result["alertActive"] = status.AlertActive;
    result["alertReason"] = GetSafetyAlarmReasonText(status);
    if (status.HasLastMeasurement) {
        result["lastValidTemperatureC"] = status.LastValidTemperatureC;
        result["lastMeasurementAgeSeconds"] =
            status.LastMeasurementAgeSeconds;
    } else {
        result["lastValidTemperatureC"] = nullptr;
        result["lastMeasurementAgeSeconds"] = nullptr;
    }
    sendReadResult(cmd, result);
    return;
}

void handleSetAlarmThreshold(const WsCommand& cmd) {
    float floatValue;
    if (!readFiniteFloat(cmd.payload, "thresholdC", floatValue) ||
        floatValue < SafetyAlarmThresholdMinC ||
        floatValue > SafetyAlarmThresholdMaxC) {
        sendError(cmd, "out_of_range",
                  "Le seuil d'alerte est absent ou hors limites.", true);
        return;
    }
    const bool persisted = SaveSafetyAlarmThreshold(floatValue);
    JsonDocument response;
    response["version"] = ProtocolVersion;
    response["type"] = "response";
    response["id"] = cmd.id;
    response["ok"] = persisted;
    JsonObject result = response["result"].to<JsonObject>();
    result["applied"] = persisted;
    result["persisted"] = persisted;
    if (!persisted) {
        response["error"]["code"] = "persistence_failed";
        response["error"]["message"] =
            "Impossible d'enregistrer le seuil d'alerte.";
    }
    String serialized;
    serializeJson(response, serialized);
    sendResponse(cmd, serialized, true);
    return;
}

}

const WsAction AlertActions[] = {
    {"getAlertLog", handleGetAlertLog},
    {"clearAlertLog", handleClearAlertLog},
    {"getAlarmSettings", handleGetAlarmSettings},
    {"setAlarmThreshold", handleSetAlarmThreshold},
};
const size_t AlertActionCount =
    sizeof(AlertActions) / sizeof(AlertActions[0]);

}
