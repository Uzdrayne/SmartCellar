#ifndef SERIAL_LINK_H
#define SERIAL_LINK_H

#include <Arduino.h>

#ifndef CELLAR_LINK_RX_PIN
#define CELLAR_LINK_RX_PIN 20
#endif

#ifndef CELLAR_LINK_TX_PIN
#define CELLAR_LINK_TX_PIN 21
#endif

struct SerialLinkSnapshot {
    bool hasReceivedTelemetry;
    bool measurementAvailable;
    bool measurementUnavailable;
    bool nanoFallback;
    bool hasLastMeasurement;
    float lastValidTemperature;
    unsigned long lastMeasurementAgeSeconds;
};

void InitializeSerialLink();
void ProcessSerialLink(unsigned long now);
void SendRelayOutputs();
SerialLinkSnapshot GetSerialLinkSnapshot(unsigned long now);

#endif
