#include "CaveController.h"
#include <Arduino.h>
#include <string.h>
#include "SerialLink/SerialLink.h"
#include "Controller/ControllerValidation.h"

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

namespace {

// Délai maximal d'attente du retour GPIO du Nano avant la première commande.
constexpr unsigned long RelayAdoptionTimeoutMs = 15000UL;
bool relaysAdopted = false;
// Statique : 800 octets hors de la pile de la boucle principale.
ControllerDebugMessages debugMessages = {};
bool controllerStarted = false;
unsigned long controllerStartedAt = 0;

unsigned long adoptedChangeTime(bool on, unsigned int minimumOnSeconds,
                                unsigned long now) {
    // Relais déjà en marche : délai min. ON considéré écoulé (pas de coupure
    // forcée). Relais à l'arrêt : le délai min. OFF repart de maintenant.
    return on ? now - static_cast<unsigned long>(minimumOnSeconds) * 1000UL : now;
}

// Reprend l'état réel des sorties pilotées par le Nano (mode secours pendant
// le redémarrage du C3) pour continuer la régulation sans couper les relais.
void adoptRelayStates(StrctDataController& data, unsigned long now) {
    if (data.RelayFeedbackValid) {
        data.ColdState = data.RelayGpioCold;
        data.HeatState = data.RelayGpioHeat;
        data.DehumState = data.RelayGpioDehum;
        data.HumState = data.RelayGpioHum;
        Serial.printf(
            "[CTRL] Reprise des relais du Nano : froid %d, chaud %d, deshum %d, hum %d, ventil %d\n",
            data.ColdState, data.HeatState, data.DehumState, data.HumState,
            data.RelayGpioFan);
    } else {
        Serial.println("[CTRL] Aucun retour des relais du Nano; démarrage à l'arrêt.");
    }

    ColdLastChangeTime = adoptedChangeTime(data.ColdState, data.ColdDelayMinOn, now);
    HeatLastChangeTime = adoptedChangeTime(data.HeatState, data.HeatDelayMinOn, now);
    DehumLastChangeTime = adoptedChangeTime(data.DehumState, data.DehumDelayMinOn, now);
    HumLastChangeTime = adoptedChangeTime(data.HumState, data.HumDelayMinOn, now);

    FanPhaseOn = data.RelayFeedbackValid ? data.RelayGpioFan : true;
    FanPhaseStartedAt = now;
    LastFanOnMinutes = data.FanOnMinutes;
    LastFanOffMinutes = data.FanOffMinutes;
    FanWasEnabled = data.FanEnabled;
    FanCycleInitialized = true;
}

}

void UpdateController() {
    const unsigned long now = millis();

    StrctDataController LocalData;

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    LocalData = DataController;
    xSemaphoreGive(DataMutex);

    if (!controllerStarted) {
        controllerStarted = true;
        controllerStartedAt = now;
    }
    if (!relaysAdopted) {
        // Aucune commande n'est envoyée au Nano tant que l'état réel est inconnu.
        if (!LocalData.RelayFeedbackValid &&
            now - controllerStartedAt < RelayAdoptionTimeoutMs) {
            return;
        }
        adoptRelayStates(LocalData, now);
        relaysAdopted = true;
    }

    const CorrectedMeasurements measurements = GetCorrectedMeasurements(LocalData);
    const float actualTemperatureCorr = measurements.Temperature;
    const float actualHumidityCorr = measurements.Humidity;
    const bool temperatureValid = measurements.TemperatureValid;
    const bool humidityValid = measurements.HumidityValid;
    const bool settingsValid = IsValidControllerConfiguration(LocalData);
    const bool controlInputsValid =
        settingsValid && temperatureValid && humidityValid;

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

    if (controlInputsValid) {
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
            debugMessages.Cold, sizeof(debugMessages.Cold),
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
            debugMessages.Heat, sizeof(debugMessages.Heat),
            "🔥", "Chauffage", "T", "°C", "Froid",
            actualTemperatureCorr,
            LocalData.SetTemp + LocalData.HeatHysLow,
            LocalData.SetTemp + LocalData.HeatHysHigh,
            false,
            heat);
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
            debugMessages.Dehum, sizeof(debugMessages.Dehum),
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
            debugMessages.Hum, sizeof(debugMessages.Hum),
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
        const char* faultReason = !settingsValid
            ? "paramètres de régulation invalides"
            : (!temperatureValid
                ? "mesure température corrigée invalide"
                : "mesure humidité corrigée invalide");
        snprintf(debugMessages.Cold, sizeof(debugMessages.Cold),
                 "⚠️ Froid — ARRÊT; %s", faultReason);
        snprintf(debugMessages.Heat, sizeof(debugMessages.Heat),
                 "⚠️ Chauffage — ARRÊT; %s", faultReason);
        snprintf(debugMessages.Dehum, sizeof(debugMessages.Dehum),
                 "⚠️ Déshumidification — ARRÊT; %s", faultReason);
        snprintf(debugMessages.Hum, sizeof(debugMessages.Hum),
                 "⚠️ Humidification — ARRÊT; %s", faultReason);
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);

    DataController.ColdState = LocalData.ColdState;
    DataController.HeatState = LocalData.HeatState;

    DataController.DehumState = LocalData.DehumState;
    DataController.HumState = LocalData.HumState;
    DataController.FanState = LocalData.FanState;
    DataController.FanPhaseElapsedSeconds = LocalData.FanPhaseElapsedSeconds;
    DataController.FanPhaseDurationSeconds = LocalData.FanPhaseDurationSeconds;
    DataDebugMessages = debugMessages;

    xSemaphoreGive(DataMutex);

    SendRelayOutputs();
}
