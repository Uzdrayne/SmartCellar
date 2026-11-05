#ifndef CONFIGURATION_LIMITS_H
#define CONFIGURATION_LIMITS_H

constexpr float MinTemperatureSetpointC = 5.0f;
constexpr float MaxTemperatureSetpointC = 30.0f;

constexpr float MinHumiditySetpointPercent = 40.0f;
constexpr float MaxHumiditySetpointPercent = 100.0f;

constexpr float MaxTemperatureOffsetC = 5.0f;
constexpr float MinTemperatureOffsetC = -MaxTemperatureOffsetC;
constexpr float MaxHumidityOffsetPercent = 10.0f;
constexpr float MinHumidityOffsetPercent = -MaxHumidityOffsetPercent;

constexpr float MaxTemperatureHysteresisC = 5.0f;
constexpr float MinColdHysteresisC = 0.0f;
constexpr float MaxColdHysteresisC = MaxTemperatureHysteresisC;
constexpr float MinHeatHysteresisC = -MaxTemperatureHysteresisC;
constexpr float MaxHeatHysteresisC = 0.0f;

constexpr float MaxHumidityHysteresisPercent = 10.0f;
constexpr float MinDehumHysteresisPercent = 0.0f;
constexpr float MaxDehumHysteresisPercent = MaxHumidityHysteresisPercent;
constexpr float MinHumHysteresisPercent = -MaxHumidityHysteresisPercent;
constexpr float MaxHumHysteresisPercent = 0.0f;

constexpr unsigned int MinRelayDelaySeconds = 5;
constexpr unsigned int MaxRelayDelaySeconds = 1800;

constexpr unsigned int FanDurationMinMinutes = 1;
constexpr unsigned int FanDurationMaxMinutes = 1440;

constexpr unsigned int MinMqttPort = 1;
constexpr unsigned int MaxMqttPort = 65535;
constexpr unsigned int DefaultMqttPort = 1883;

constexpr float SafetyAlarmThresholdMinC = 0.0f;
constexpr float SafetyAlarmThresholdMaxC = 40.0f;

#endif
