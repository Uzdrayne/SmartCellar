#include "Controller/ControllerValidation.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>

namespace {

bool hasOnlyTrailingWhitespace(const char* text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        ++text;
    }
    return *text == '\0';
}

bool isValidRelayDelay(unsigned int seconds) {
    return seconds >= MinRelayDelaySeconds && seconds <= MaxRelayDelaySeconds;
}

bool isValidTemperatureHysteresis(const StrctDataController& data) {
    return isfinite(data.ColdHysHigh) && isfinite(data.ColdHysLow) &&
        isfinite(data.HeatHysHigh) && isfinite(data.HeatHysLow) &&
        data.ColdHysLow >= 0.0f &&
        data.ColdHysHigh <= MaxTemperatureHysteresisC &&
        data.ColdHysHigh > data.ColdHysLow &&
        data.HeatHysHigh <= 0.0f &&
        data.HeatHysLow >= -MaxTemperatureHysteresisC &&
        data.HeatHysHigh > data.HeatHysLow &&
        data.ColdHysLow > data.HeatHysHigh;
}

bool isValidHumidityHysteresis(const StrctDataController& data) {
    return isfinite(data.DehumHysHigh) && isfinite(data.DehumHysLow) &&
        isfinite(data.HumHysHigh) && isfinite(data.HumHysLow) &&
        data.DehumHysLow >= 0.0f &&
        data.DehumHysHigh <= MaxHumidityHysteresisPercent &&
        data.DehumHysHigh > data.DehumHysLow &&
        data.HumHysHigh <= 0.0f &&
        data.HumHysLow >= -MaxHumidityHysteresisPercent &&
        data.HumHysHigh > data.HumHysLow &&
        data.DehumHysLow > data.HumHysHigh;
}

}

bool ParseFiniteFloat(const String& text, float& value) {
    const char* start = text.c_str();
    char* end = nullptr;
    errno = 0;
    const float parsed = strtof(start, &end);

    if (start == end || errno == ERANGE || !hasOnlyTrailingWhitespace(end) ||
        !isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

bool ParseBoundedUnsigned(const String& text, unsigned int minimum,
                          unsigned int maximum, unsigned int& value) {
    const char* start = text.c_str();
    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(start, &end, 10);

    if (start == end || errno == ERANGE || !hasOnlyTrailingWhitespace(end) ||
        parsed < static_cast<long>(minimum) ||
        parsed > static_cast<long>(maximum)) {
        return false;
    }

    value = static_cast<unsigned int>(parsed);
    return true;
}

bool ParseBoolean(const String& text, bool& value) {
    if (text == "1" || text == "true") {
        value = true;
        return true;
    }
    if (text == "0" || text == "false") {
        value = false;
        return true;
    }
    return false;
}

bool IsValidTemperatureSetpoint(float value) {
    return isfinite(value) &&
        value >= MinTemperatureSetpointC &&
        value <= MaxTemperatureSetpointC;
}

bool IsValidHumiditySetpoint(float value) {
    return isfinite(value) &&
        value >= MinHumiditySetpointPercent &&
        value <= MaxHumiditySetpointPercent;
}

bool IsValidTemperatureOffset(float value) {
    return isfinite(value) && fabsf(value) <= MaxTemperatureOffsetC;
}

bool IsValidHumidityOffset(float value) {
    return isfinite(value) && fabsf(value) <= MaxHumidityOffsetPercent;
}

bool IsValidSetpoints(float temperature, float humidity) {
    return IsValidTemperatureSetpoint(temperature) &&
        IsValidHumiditySetpoint(humidity);
}

bool IsValidTemperatureRelaySettings(const StrctDataController& data) {
    return isValidTemperatureHysteresis(data) &&
        isValidRelayDelay(data.ColdDelayMinOn) &&
        isValidRelayDelay(data.ColdDelayMinOff) &&
        isValidRelayDelay(data.HeatDelayMinOn) &&
        isValidRelayDelay(data.HeatDelayMinOff);
}

bool IsValidHumidityRelaySettings(const StrctDataController& data) {
    return isValidHumidityHysteresis(data) &&
        isValidRelayDelay(data.DehumDelayMinOn) &&
        isValidRelayDelay(data.DehumDelayMinOff) &&
        isValidRelayDelay(data.HumDelayMinOn) &&
        isValidRelayDelay(data.HumDelayMinOff);
}

bool IsValidControllerConfiguration(const StrctDataController& data) {
    return IsValidSetpoints(data.SetTemp, data.SetHum) &&
        IsValidTemperatureOffset(data.TempOffset) &&
        IsValidHumidityOffset(data.HumOffset) &&
        IsValidTemperatureRelaySettings(data) &&
        IsValidHumidityRelaySettings(data);
}
