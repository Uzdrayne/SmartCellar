#ifndef CONTROLLER_VALIDATION_H
#define CONTROLLER_VALIDATION_H

#include <Arduino.h>

#include "Data/ControllerData.h"

constexpr float MinTemperatureSetpointC = 5.0f;
constexpr float MaxTemperatureSetpointC = 30.0f;
constexpr float MinHumiditySetpointPercent = 40.0f;
constexpr float MaxHumiditySetpointPercent = 100.0f;
constexpr float MaxTemperatureOffsetC = 5.0f;
constexpr float MaxHumidityOffsetPercent = 10.0f;
constexpr float MaxTemperatureHysteresisC = 5.0f;
constexpr float MaxHumidityHysteresisPercent = 10.0f;
constexpr unsigned int MinRelayDelaySeconds = 5;
constexpr unsigned int MaxRelayDelaySeconds = 1800;

bool ParseFiniteFloat(const String& text, float& value);
bool ParseBoundedUnsigned(const String& text, unsigned int minimum,
                          unsigned int maximum, unsigned int& value);
bool ParseBoolean(const String& text, bool& value);

bool IsValidTemperatureSetpoint(float value);
bool IsValidHumiditySetpoint(float value);
bool IsValidTemperatureOffset(float value);
bool IsValidHumidityOffset(float value);
bool IsValidSetpoints(float temperature, float humidity);
bool IsValidTemperatureRelaySettings(const StrctDataController& data);
bool IsValidHumidityRelaySettings(const StrctDataController& data);
bool IsValidControllerConfiguration(const StrctDataController& data);

#endif
