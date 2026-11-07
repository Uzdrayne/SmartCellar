#include "Alarm/AlertManager.h"

#include "Alarm/SafetyAlarm.h"
#include "Data/ControllerData.h"
#include "Global/Global.h"
#include "SerialLink/SerialLink.h"

AlertSnapshot GetAlertSnapshot(const unsigned long now) {
    const SafetyAlarmStatus safetyStatus = GetSafetyAlarmStatus();
    const SerialLinkSnapshot serialStatus = GetSerialLinkSnapshot(now);
    const bool wifiIsSta = Get_WifiIsSTA();
    bool wifiConnected;
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    wifiConnected = DataWifi.WifiConnected;
    xSemaphoreGive(DataMutex);

    AlertSnapshot status = {};
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
    return status;
}
