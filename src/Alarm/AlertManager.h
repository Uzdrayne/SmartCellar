#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>

struct AlertSnapshot {
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

AlertSnapshot GetAlertSnapshot(unsigned long now);

#endif
