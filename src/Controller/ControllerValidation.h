#ifndef CONTROLLER_VALIDATION_H
#define CONTROLLER_VALIDATION_H

#include <Arduino.h>

#include "Controller/ConfigurationLimits.h"
#include "Data/ControllerData.h"

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
