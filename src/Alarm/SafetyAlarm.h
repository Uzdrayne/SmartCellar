#ifndef SAFETY_ALARM_H
#define SAFETY_ALARM_H

#include <Arduino.h>

constexpr float SafetyAlarmThresholdMinC = 0.0f;
constexpr float SafetyAlarmThresholdMaxC = 40.0f;
constexpr float SafetyAlarmThresholdDefaultC = 25.0f;
constexpr unsigned long SafetyAlarmTriggerDelaySeconds = 300;
constexpr unsigned long SafetyAlarmRecoveryDelaySeconds = 30;

struct SafetyAlarmConfiguration {
    float HighTemperatureThresholdC;
};

struct SafetyAlarmStatus {
    bool AlertActive;
    bool HighTemperatureActive;
    bool MeasurementsUnavailable;
    bool NanoFallback;
    bool HasLastMeasurement;
    float LastValidTemperatureC;
    unsigned long LastMeasurementAgeSeconds;
};

bool InitializeSafetyAlarm();
bool SaveSafetyAlarmThreshold(float thresholdC);
SafetyAlarmConfiguration GetSafetyAlarmConfiguration();
SafetyAlarmStatus GetSafetyAlarmStatus();
String GetSafetyAlarmReasonText(const SafetyAlarmStatus& status);
void UpdateSafetyAlarm(unsigned long now);

#endif
