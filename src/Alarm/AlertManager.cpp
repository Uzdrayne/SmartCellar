#include "Alarm/AlertManager.h"

#include "Alarm/SafetyAlarm.h"
#include "Data/ControllerData.h"
#include "Global/Global.h"
#include "SerialLink/SerialLink.h"

namespace {

bool previousPrimaryAlertActive = false;
PrimaryAlertReason lastPrimaryAlertReason = PrimaryAlertReason::None;
unsigned long lastPrimaryAlertRaisedAtMs = 0;
AlertSnapshot currentAlertSnapshot = {};

PrimaryAlertReason getActivePrimaryAlertReason(const AlertSnapshot& status) {
    if (status.NanoLinkLost) {
        return PrimaryAlertReason::NanoLinkLost;
    }
    if (status.MeasurementsUnavailable) {
        return PrimaryAlertReason::MeasurementsUnavailable;
    }
    if (status.HighTemperatureActive) {
        return PrimaryAlertReason::HighTemperature;
    }
    if (status.NanoFallback) {
        return PrimaryAlertReason::NanoFallback;
    }
    return PrimaryAlertReason::None;
}

}

void UpdateAlertManager(const unsigned long now) {
    const SafetyAlarmStatus safetyStatus = GetSafetyAlarmStatus();
    const SerialLinkSnapshot serialStatus = GetSerialLinkSnapshot(now);
    const bool wifiIsSta = Get_WifiIsSTA();
    bool wifiConnected;
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    wifiConnected = DataWifi.WifiConnected;
    xSemaphoreGive(DataMutex);

    AlertSnapshot status = {};
    status.NanoTelemetryReceived = serialStatus.hasReceivedTelemetry;
    status.TelemetryAvailable = serialStatus.measurementAvailable;
    status.NanoLinkLost = serialStatus.measurementUnavailable;
    status.MeasurementsUnavailable =
        safetyStatus.MeasurementsUnavailable && !status.NanoLinkLost;
    status.HighTemperatureActive = safetyStatus.HighTemperatureActive;
    status.NanoFallback = safetyStatus.NanoFallback;
    status.WifiDisconnected = wifiIsSta && !wifiConnected;
    status.MqttDisconnected = Get_MqttEnable() && !Get_MqttConnected();
    status.PrimaryAlertActive =
        status.NanoLinkLost ||
        status.MeasurementsUnavailable ||
        status.HighTemperatureActive ||
        status.NanoFallback;
    status.AnyAlertActive =
        status.PrimaryAlertActive ||
        status.WifiDisconnected ||
        status.MqttDisconnected;
    status.HasLastMeasurement = safetyStatus.HasLastMeasurement;
    status.LastValidTemperatureC = safetyStatus.LastValidTemperatureC;
    status.LastMeasurementAgeSeconds = safetyStatus.LastMeasurementAgeSeconds;

    if (status.PrimaryAlertActive && !previousPrimaryAlertActive) {
        const PrimaryAlertReason reason = getActivePrimaryAlertReason(status);
        if (reason != PrimaryAlertReason::None) {
            lastPrimaryAlertReason = reason;
            lastPrimaryAlertRaisedAtMs = now;
        }
    }
    previousPrimaryAlertActive = status.PrimaryAlertActive;
    status.LastPrimaryAlertReason = lastPrimaryAlertReason;
    status.LastPrimaryAlertRaisedAtMs = lastPrimaryAlertRaisedAtMs;

    currentAlertSnapshot = status;
}

AlertSnapshot GetAlertSnapshot() {
    return currentAlertSnapshot;
}
