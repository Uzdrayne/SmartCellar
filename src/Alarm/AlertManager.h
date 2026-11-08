#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>

enum class PrimaryAlertReason : uint8_t {
    None,
    NanoLinkLost,
    MeasurementsUnavailable,
    HighTemperature,
    NanoFallback
};

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
    PrimaryAlertReason LastPrimaryAlertReason;
    unsigned long LastPrimaryAlertRaisedAtMs;
};

void UpdateAlertManager(unsigned long now);
AlertSnapshot GetAlertSnapshot();

#endif
