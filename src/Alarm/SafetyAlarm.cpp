#include "Alarm/SafetyAlarm.h"

#include <Preferences.h>
#include <math.h>

#include "Data/ControllerData.h"
#include "SerialLink/SerialLink.h"

namespace {

constexpr char AlarmPreferencesNamespace[] = "SafetyAlarm";
constexpr char HighTemperatureThresholdKey[] = "HighTemp";
constexpr unsigned long HighTemperatureDelayMs =
    SafetyAlarmTriggerDelaySeconds * 1000UL;
constexpr unsigned long RecoveryDelayMs =
    SafetyAlarmRecoveryDelaySeconds * 1000UL;

SafetyAlarmConfiguration alarmConfiguration = {
    SafetyAlarmThresholdDefaultC
};
SafetyAlarmStatus alarmStatus = {};
bool highTemperatureTiming = false;
bool recoveryTiming = false;
unsigned long highTemperatureStartedAt = 0;
unsigned long recoveryStartedAt = 0;

bool isValidThreshold(float thresholdC) {
    return isfinite(thresholdC) &&
        thresholdC >= SafetyAlarmThresholdMinC &&
        thresholdC <= SafetyAlarmThresholdMaxC;
}

}

bool InitializeSafetyAlarm() {
    Preferences preferences;
    if (!preferences.begin(AlarmPreferencesNamespace, false)) {
        Serial.println("[SafetyAlarm] Impossible d'ouvrir la configuration NVS");
        return false;
    }

    const bool thresholdExists = preferences.isKey(HighTemperatureThresholdKey);
    const float storedThreshold = preferences.getFloat(
        HighTemperatureThresholdKey, SafetyAlarmThresholdDefaultC);
    const float threshold = thresholdExists && isValidThreshold(storedThreshold)
        ? storedThreshold
        : SafetyAlarmThresholdDefaultC;

    bool saved = true;
    if (!thresholdExists || !isValidThreshold(storedThreshold)) {
        saved = preferences.putFloat(HighTemperatureThresholdKey, threshold) ==
            sizeof(float);
    }
    preferences.end();

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    alarmConfiguration.HighTemperatureThresholdC = threshold;
    alarmStatus = {};
    highTemperatureTiming = false;
    recoveryTiming = false;
    xSemaphoreGive(DataMutex);

    if (!saved) {
        Serial.println("[SafetyAlarm] Échec d'initialisation du seuil par défaut");
    }
    return saved;
}

bool SaveSafetyAlarmThreshold(float thresholdC) {
    if (!isValidThreshold(thresholdC)) {
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(AlarmPreferencesNamespace, false)) {
        Serial.println("[SafetyAlarm] Impossible d'ouvrir la configuration NVS");
        return false;
    }

    const size_t written =
        preferences.putFloat(HighTemperatureThresholdKey, thresholdC);
    preferences.end();
    if (written != sizeof(float)) {
        Serial.println("[SafetyAlarm] Échec de sauvegarde du seuil de température");
        return false;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    alarmConfiguration.HighTemperatureThresholdC = thresholdC;
    highTemperatureTiming = false;
    recoveryTiming = false;
    xSemaphoreGive(DataMutex);
    return true;
}

SafetyAlarmConfiguration GetSafetyAlarmConfiguration() {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const SafetyAlarmConfiguration configuration = alarmConfiguration;
    xSemaphoreGive(DataMutex);
    return configuration;
}

SafetyAlarmStatus GetSafetyAlarmStatus() {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const SafetyAlarmStatus status = alarmStatus;
    xSemaphoreGive(DataMutex);
    return status;
}

String GetSafetyAlarmReasonText(const SafetyAlarmStatus& status) {
    String reason;
    if (status.NanoFallback) {
        reason = "Mode secours";
    }
    if (status.MeasurementsUnavailable) {
        if (!reason.isEmpty()) {
            reason += " ; ";
        }
        reason += "Mesures ou liaison Nano indisponibles";
    }
    if (status.HighTemperatureActive) {
        if (!reason.isEmpty()) {
            reason += " ; ";
        }
        reason += "Température élevée";
    }
    if (reason.isEmpty()) {
        reason = status.HasLastMeasurement ? "Normal" : "Initialisation";
    }
    return reason;
}

void UpdateSafetyAlarm(unsigned long now) {
    const SerialLinkSnapshot linkStatus = GetSerialLinkSnapshot(now);

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    const float correctedTemperature =
        DataController.Temperature +
        (DataController.TempOffsetEnabled ? DataController.TempOffset : 0.0f);
    const bool correctedTemperatureValid =
        linkStatus.measurementAvailable &&
        isfinite(DataController.Temperature) &&
        isfinite(correctedTemperature) &&
        correctedTemperature >= -40.0f &&
        correctedTemperature <= 80.0f;

    if (correctedTemperatureValid &&
        correctedTemperature > alarmConfiguration.HighTemperatureThresholdC) {
        recoveryTiming = false;
        if (!alarmStatus.HighTemperatureActive) {
            if (!highTemperatureTiming) {
                highTemperatureTiming = true;
                highTemperatureStartedAt = now;
            } else if (now - highTemperatureStartedAt >= HighTemperatureDelayMs) {
                alarmStatus.HighTemperatureActive = true;
                highTemperatureTiming = false;
            }
        } else {
            highTemperatureTiming = false;
        }
    } else if (correctedTemperatureValid &&
               correctedTemperature < alarmConfiguration.HighTemperatureThresholdC) {
        highTemperatureTiming = false;
        if (alarmStatus.HighTemperatureActive) {
            if (!recoveryTiming) {
                recoveryTiming = true;
                recoveryStartedAt = now;
            } else if (now - recoveryStartedAt >= RecoveryDelayMs) {
                alarmStatus.HighTemperatureActive = false;
                recoveryTiming = false;
            }
        } else {
            recoveryTiming = false;
        }
    } else {
        highTemperatureTiming = false;
        recoveryTiming = false;
    }

    alarmStatus.MeasurementsUnavailable =
        linkStatus.measurementUnavailable ||
        (linkStatus.measurementAvailable && !correctedTemperatureValid);
    alarmStatus.NanoFallback = linkStatus.nanoFallback;
    alarmStatus.HasLastMeasurement = linkStatus.hasLastMeasurement;
    alarmStatus.LastValidTemperatureC = linkStatus.hasLastMeasurement
        ? linkStatus.lastValidTemperature +
            (DataController.TempOffsetEnabled ? DataController.TempOffset : 0.0f)
        : NAN;
    alarmStatus.LastMeasurementAgeSeconds = linkStatus.lastMeasurementAgeSeconds;
    alarmStatus.AlertActive =
        alarmStatus.HighTemperatureActive ||
        alarmStatus.MeasurementsUnavailable ||
        alarmStatus.NanoFallback;
    xSemaphoreGive(DataMutex);
}
