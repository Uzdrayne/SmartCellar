#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>

struct AlertSnapshot {
    bool NanoTelemetryReceived;
    bool TelemetryAvailable;
    bool NanoLinkLost;
    bool MeasurementsUnavailable;
    bool HighTemperatureActive;
    bool NanoFallback;
    bool WifiDisconnected;
    bool MqttDisconnected;
    bool PrimaryAlertActive;
    bool AnyAlertActive;
    bool HasLastMeasurement;
    float LastValidTemperatureC;
    unsigned long LastMeasurementAgeSeconds;
};

void UpdateAlertManager(unsigned long now);
AlertSnapshot GetAlertSnapshot();

#endif
