#include "CaveController.h"
#include <Arduino.h>
#include <string.h>
#include "SerialLink/SerialLink.h"

namespace {

enum class RelayReason : uint8_t {
    NoDemand,
    Maintaining,
    WaitingMinOn,
    WaitingMinOff,
    WaitingForOpposite,
    Started,
    Stopped
};

struct RelayDecision {
    bool state;
    RelayReason reason;
    unsigned long remainingSeconds;
};

RelayDecision evaluateRelay(
    bool currentState,
    float measurement,
    float activationThreshold,
    float deactivationThreshold,
    bool activatesAbove,
    unsigned long elapsedMs,
    unsigned int minimumOnSeconds,
    unsigned int minimumOffSeconds,
    bool oppositeIsOn,
    bool interlockRequired,
    unsigned long oppositeElapsedMs,
    unsigned int oppositeMinimumOnSeconds) {

    const bool transitionRequested = currentState
        ? (activatesAbove ? measurement < deactivationThreshold : measurement >= deactivationThreshold)
        : (activatesAbove ? measurement > activationThreshold : measurement < activationThreshold);

    if (!transitionRequested) {
        return {
            currentState,
            currentState ? RelayReason::Maintaining : RelayReason::NoDemand,
            0
        };
    }

    const unsigned int minimumSeconds = currentState ? minimumOnSeconds : minimumOffSeconds;
    const unsigned long minimumMs = static_cast<unsigned long>(minimumSeconds) * 1000UL;
    if (elapsedMs < minimumMs) {
        return {
            currentState,
            currentState ? RelayReason::WaitingMinOn : RelayReason::WaitingMinOff,
            (minimumMs - elapsedMs) / 1000UL
        };
    }

    if (!currentState && oppositeIsOn && interlockRequired) {
        const unsigned long oppositeMinimumMs =
            static_cast<unsigned long>(oppositeMinimumOnSeconds) * 1000UL;
        const unsigned long oppositeRemainingMs = oppositeElapsedMs < oppositeMinimumMs
            ? oppositeMinimumMs - oppositeElapsedMs
            : 0;
        return {
            false,
            RelayReason::WaitingForOpposite,
            oppositeRemainingMs / 1000UL
        };
    }

    return {
        !currentState,
        currentState ? RelayReason::Stopped : RelayReason::Started,
        0
    };
}

void formatRelayMessage(
    char* message,
    size_t messageSize,
    const char* icon,
    const char* relayName,
    const char* measurementName,
    const char* unit,
    const char* oppositeRelayName,
    float measurement,
    float activationThreshold,
    float deactivationThreshold,
    bool activatesAbove,
    const RelayDecision& decision) {

    const char* activationOperator = activatesAbove ? ">" : "<";
    const char* deactivationOperator = activatesAbove ? "<" : ">=";

    switch (decision.reason) {
        case RelayReason::NoDemand:
            snprintf(message, messageSize,
                     "%s %s — ARRÊT; pas de demande, démarrage si %s %s %.2f%s (mesuré %.2f%s)",
                     icon, relayName, measurementName, activationOperator,
                     activationThreshold, unit, measurement, unit);
            break;
        case RelayReason::Maintaining:
            snprintf(message, messageSize,
                     "%s %s — EN MARCHE; maintien, arrêt si %s %s %.2f%s (mesuré %.2f%s)",
                     icon, relayName, measurementName, deactivationOperator,
                     deactivationThreshold, unit, measurement, unit);
            break;
        case RelayReason::WaitingMinOn:
            snprintf(message, messageSize,
                     "%s %s — EN MARCHE; arrêt demandé (%.2f%s; arrêt si %s %s %.2f%s), délai min. ON : %lu s",
                     icon, relayName, measurement, unit, measurementName, deactivationOperator,
                     deactivationThreshold, unit,
                     decision.remainingSeconds);
            break;
        case RelayReason::WaitingMinOff:
            snprintf(message, messageSize,
                     "%s %s — ARRÊT; démarrage demandé (%.2f%s; démarre si %s %s %.2f%s), délai min. OFF : %lu s",
                     icon, relayName, measurement, unit, measurementName, activationOperator,
                     activationThreshold, unit,
                     decision.remainingSeconds);
            break;
        case RelayReason::WaitingForOpposite:
            if (decision.remainingSeconds > 0) {
                snprintf(message, messageSize,
                         "%s %s — ARRÊT; démarrage demandé (%.2f%s; seuil ON %s %.2f%s), attendre arrêt %s (min. ON restant : %lu s)",
                         icon, relayName, measurement, unit, activationOperator, activationThreshold, unit,
                         oppositeRelayName, decision.remainingSeconds);
            } else {
                snprintf(message, messageSize,
                         "%s %s — ARRÊT; démarrage demandé (%.2f%s; seuil ON %s %.2f%s), attendre le seuil d'arrêt de %s",
                         icon, relayName, measurement, unit, activationOperator, activationThreshold, unit,
                         oppositeRelayName);
            }
            break;
        case RelayReason::Started:
            snprintf(message, messageSize,
                     "%s %s — EN MARCHE; activation, %s %.2f%s %s seuil %.2f%s",
                     icon, relayName, measurementName, measurement, unit,
                     activationOperator, activationThreshold, unit);
            break;
        case RelayReason::Stopped:
            snprintf(message, messageSize,
                     "%s %s — ARRÊT; arrêt, %s %.2f%s %s seuil %.2f%s",
                     icon, relayName, measurementName, measurement, unit,
                     deactivationOperator, deactivationThreshold, unit);
            break;
    }
    message[messageSize - 1] = '\0';
    message[messageSize - 1] = '\0';
}

}

// timestamp dernière commutation
unsigned long ColdLastChangeTime = 0;
unsigned long HeatLastChangeTime = 0;
unsigned long DehumLastChangeTime = 0;
unsigned long HumLastChangeTime = 0;
unsigned long FanPhaseStartedAt = 0;
unsigned int LastFanOnMinutes = 0;
unsigned int LastFanOffMinutes = 0;
bool FanCycleInitialized = false;
bool FanWasEnabled = false;
bool FanPhaseOn = true;

char N_ColdDebugMsg[200] = "";
char LastColdDebugMsg[200] = "";

char N_HeatDebugMsg[200] = "";
char LastHeatDebugMsg[200] = "";

char N_DehumDebugMsg[200] = "";
char LastDehumDebugMsg[200] = "";

char N_HumDebugMsg[200] = "";
char LastHumDebugMsg[200] = "";

void applyRelays();

void UpdateController() {
    const unsigned long now = millis();

    StrctDataController LocalData;

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    LocalData = DataController;
    xSemaphoreGive(DataMutex);

    const float actualTemperatureCorr =
        LocalData.Temperature + (LocalData.TempOffsetEnabled ? LocalData.TempOffset : 0.0f);
    const float actualHumidityCorr =
        LocalData.Humidity + (LocalData.HumOffsetEnabled ? LocalData.HumOffset : 0.0f);

    const unsigned long ColdElapsed = now - ColdLastChangeTime;
    const unsigned long HeatElapsed = now - HeatLastChangeTime;
    const unsigned long DehumElapsed = now - DehumLastChangeTime;
    const unsigned long HumElapsed = now - HumLastChangeTime;

    if (!FanCycleInitialized ||
        LocalData.FanOnMinutes != LastFanOnMinutes ||
        LocalData.FanOffMinutes != LastFanOffMinutes ||
        (LocalData.FanEnabled && !FanWasEnabled)) {
        FanPhaseOn = true;
        FanPhaseStartedAt = now;
        LastFanOnMinutes = LocalData.FanOnMinutes;
        LastFanOffMinutes = LocalData.FanOffMinutes;
        FanCycleInitialized = true;
    }

    if (LocalData.FanEnabled) {
        unsigned long phaseDurationMs =
            static_cast<unsigned long>(FanPhaseOn ? LocalData.FanOnMinutes : LocalData.FanOffMinutes) * 60000UL;
        if (now - FanPhaseStartedAt >= phaseDurationMs) {
            FanPhaseOn = !FanPhaseOn;
            FanPhaseStartedAt = now;
            phaseDurationMs =
                static_cast<unsigned long>(FanPhaseOn ? LocalData.FanOnMinutes : LocalData.FanOffMinutes) * 60000UL;
        }
        LocalData.FanState = FanPhaseOn;
        LocalData.FanPhaseElapsedSeconds = (now - FanPhaseStartedAt) / 1000UL;
        LocalData.FanPhaseDurationSeconds = phaseDurationMs / 1000UL;
    } else {
        LocalData.FanState = false;
        LocalData.FanPhaseElapsedSeconds = 0;
        LocalData.FanPhaseDurationSeconds = 0;
    }
    FanWasEnabled = LocalData.FanEnabled;

    if (!isnan(LocalData.Temperature)) {
        const RelayDecision cold = evaluateRelay(
            LocalData.ColdState,
            actualTemperatureCorr,
            LocalData.SetTemp + LocalData.ColdHysHigh,
            LocalData.SetTemp + LocalData.ColdHysLow,
            true,
            ColdElapsed,
            LocalData.ColdDelayMinOn,
            LocalData.ColdDelayMinOff,
            LocalData.HeatState,
            LocalData.ColdHysLow > LocalData.HeatHysHigh,
            HeatElapsed,
            LocalData.HeatDelayMinOn);
        if (cold.state != LocalData.ColdState) {
            ColdLastChangeTime = now;
        }
        LocalData.ColdState = cold.state;
        formatRelayMessage(
            LocalData.ColdDebugMsg, sizeof(LocalData.ColdDebugMsg),
            "❄️", "Froid", "T", "°C", "Chauffage",
            actualTemperatureCorr,
            LocalData.SetTemp + LocalData.ColdHysHigh,
            LocalData.SetTemp + LocalData.ColdHysLow,
            true,
            cold);

        const RelayDecision heat = evaluateRelay(
            LocalData.HeatState,
            actualTemperatureCorr,
            LocalData.SetTemp + LocalData.HeatHysLow,
            LocalData.SetTemp + LocalData.HeatHysHigh,
            false,
            HeatElapsed,
            LocalData.HeatDelayMinOn,
            LocalData.HeatDelayMinOff,
            LocalData.ColdState,
            LocalData.ColdHysLow > LocalData.HeatHysHigh,
            ColdElapsed,
            LocalData.ColdDelayMinOn);
        if (heat.state != LocalData.HeatState) {
            HeatLastChangeTime = now;
        }
        LocalData.HeatState = heat.state;
        formatRelayMessage(
            LocalData.HeatDebugMsg, sizeof(LocalData.HeatDebugMsg),
            "🔥", "Chauffage", "T", "°C", "Froid",
            actualTemperatureCorr,
            LocalData.SetTemp + LocalData.HeatHysLow,
            LocalData.SetTemp + LocalData.HeatHysHigh,
            false,
            heat);
    } else {
        LocalData.ColdState = false;
        LocalData.HeatState = false;
        snprintf(LocalData.ColdDebugMsg, sizeof(LocalData.ColdDebugMsg),
                 "⚠️ Froid — ARRÊT; capteur de température en erreur");
        snprintf(LocalData.HeatDebugMsg, sizeof(LocalData.HeatDebugMsg),
                 "⚠️ Chauffage — ARRÊT; capteur de température en erreur");
    }

    if (!isnan(LocalData.Humidity)) {
        const RelayDecision dehum = evaluateRelay(
            LocalData.DehumState,
            actualHumidityCorr,
            LocalData.SetHum + LocalData.DehumHysHigh,
            LocalData.SetHum + LocalData.DehumHysLow,
            true,
            DehumElapsed,
            LocalData.DehumDelayMinOn,
            LocalData.DehumDelayMinOff,
            LocalData.HumState,
            LocalData.DehumHysLow > LocalData.HumHysHigh,
            HumElapsed,
            LocalData.HumDelayMinOn);
        if (dehum.state != LocalData.DehumState) {
            DehumLastChangeTime = now;
        }
        LocalData.DehumState = dehum.state;
        formatRelayMessage(
            LocalData.DehumDebugMsg, sizeof(LocalData.DehumDebugMsg),
            "💨", "Déshumidification", "HR", "%", "Humidification",
            actualHumidityCorr,
            LocalData.SetHum + LocalData.DehumHysHigh,
            LocalData.SetHum + LocalData.DehumHysLow,
            true,
            dehum);

        const RelayDecision hum = evaluateRelay(
            LocalData.HumState,
            actualHumidityCorr,
            LocalData.SetHum + LocalData.HumHysLow,
            LocalData.SetHum + LocalData.HumHysHigh,
            false,
            HumElapsed,
            LocalData.HumDelayMinOn,
            LocalData.HumDelayMinOff,
            LocalData.DehumState,
            LocalData.DehumHysLow > LocalData.HumHysHigh,
            DehumElapsed,
            LocalData.DehumDelayMinOn);
        if (hum.state != LocalData.HumState) {
            HumLastChangeTime = now;
        }
        LocalData.HumState = hum.state;
        formatRelayMessage(
            LocalData.HumDebugMsg, sizeof(LocalData.HumDebugMsg),
            "💦", "Humidification", "HR", "%", "Déshumidification",
            actualHumidityCorr,
            LocalData.SetHum + LocalData.HumHysLow,
            LocalData.SetHum + LocalData.HumHysHigh,
            false,
            hum);
    } else {
        if (LocalData.ColdState) ColdLastChangeTime = now;
        if (LocalData.HeatState) HeatLastChangeTime = now;
        if (LocalData.DehumState) DehumLastChangeTime = now;
        if (LocalData.HumState) HumLastChangeTime = now;
        LocalData.ColdState = false;
        LocalData.HeatState = false;
        LocalData.DehumState = false;
        LocalData.HumState = false;
        snprintf(LocalData.ColdDebugMsg, sizeof(LocalData.ColdDebugMsg),
                 "⚠️ Froid — ARRÊT; capteur d'humidité en erreur");
        snprintf(LocalData.HeatDebugMsg, sizeof(LocalData.HeatDebugMsg),
                 "⚠️ Chauffage — ARRÊT; capteur d'humidité en erreur");
        snprintf(LocalData.DehumDebugMsg, sizeof(LocalData.DehumDebugMsg),
                 "⚠️ Déshumidification — ARRÊT; capteur d'humidité en erreur");
        snprintf(LocalData.HumDebugMsg, sizeof(LocalData.HumDebugMsg),
                 "⚠️ Humidification — ARRÊT; capteur d'humidité en erreur");
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.ColdState = LocalData.ColdState;
    DataController.HeatState = LocalData.HeatState;
    strncpy(DataController.ColdDebugMsg, LocalData.ColdDebugMsg, sizeof(DataController.ColdDebugMsg));
    strncpy(DataController.HeatDebugMsg, LocalData.HeatDebugMsg, sizeof(DataController.HeatDebugMsg));

    DataController.DehumState = LocalData.DehumState;
    DataController.HumState = LocalData.HumState;
    DataController.FanState = LocalData.FanState;
    DataController.FanPhaseElapsedSeconds = LocalData.FanPhaseElapsedSeconds;
    DataController.FanPhaseDurationSeconds = LocalData.FanPhaseDurationSeconds;
    strncpy(DataController.DehumDebugMsg, LocalData.DehumDebugMsg, sizeof(DataController.DehumDebugMsg));
    strncpy(DataController.HumDebugMsg, LocalData.HumDebugMsg, sizeof(DataController.HumDebugMsg));

    xSemaphoreGive(DataMutex);

    applyRelays();
}

void PrintControllerMsgs() {
    if (strcmp(N_ColdDebugMsg, LastColdDebugMsg) != 0) {
        Serial.println(N_ColdDebugMsg);
        strncpy(LastColdDebugMsg, N_ColdDebugMsg, sizeof(LastColdDebugMsg));
        LastColdDebugMsg[sizeof(LastColdDebugMsg) - 1] = '\0';
    }

    if (strcmp(N_HeatDebugMsg, LastHeatDebugMsg) != 0) {
        Serial.println(N_HeatDebugMsg);
        strncpy(LastHeatDebugMsg, N_HeatDebugMsg, sizeof(LastHeatDebugMsg));
        LastHeatDebugMsg[sizeof(LastHeatDebugMsg) - 1] = '\0';
    }
    if (strcmp(N_DehumDebugMsg, LastDehumDebugMsg) != 0) {
        Serial.println(N_DehumDebugMsg);
        strncpy(LastDehumDebugMsg, N_DehumDebugMsg, sizeof(LastDehumDebugMsg));
        LastDehumDebugMsg[sizeof(LastDehumDebugMsg) - 1] = '\0';
    }
    if (strcmp(N_HumDebugMsg, LastHumDebugMsg) != 0) {
        Serial.println(N_HumDebugMsg);
        strncpy(LastHumDebugMsg, N_HumDebugMsg, sizeof(LastHumDebugMsg));
        LastHumDebugMsg[sizeof(LastHumDebugMsg) - 1] = '\0';
    }
}

void applyRelays() {
    SendRelayOutputs();
}
