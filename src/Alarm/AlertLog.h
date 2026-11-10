#ifndef ALERT_LOG_H
#define ALERT_LOG_H

#include <Arduino.h>

struct AlertSnapshot;

// Types d'alertes et d'événements journalisés. L'ordre des alertes continues
// est l'ordre d'affichage (écran et Web).
enum class AlertType : uint8_t {
    NanoLinkLost,
    MeasurementsUnavailable,
    HighTemperature,
    NanoFallback,
    WifiDisconnected,
    MqttDisconnected,
    AbnormalRestart,
    SaveFailed,
    Count
};

constexpr size_t AlertLogCapacity = 10;
constexpr size_t AlertTypeCount = static_cast<size_t>(AlertType::Count);

struct AlertLogEntry {
    AlertType Type;
    bool Active;
    uint8_t Occurrences;
    uint32_t RaisedAtMs;
    // Alerte en cours : dernière (ré)activation. Alerte terminée : heure de fin.
    uint32_t LastChangeMs;
    // Cause du redémarrage (esp_reset_reason_t) pour AbnormalRestart.
    uint8_t Detail;
};

// Code stable pour le Web/API (ex. "nano_link_lost").
const char* GetAlertTypeCode(AlertType type);
// Libellé court sans accents, adapté à la police de l'écran.
const char* GetAlertTypeLabel(AlertType type);
bool IsPrimaryAlertType(AlertType type);
// Alertes en cours dans l'instantané, dans l'ordre d'affichage.
size_t GetActiveAlertTypes(const AlertSnapshot& snapshot, AlertType* types,
                           size_t maxTypes);

void InitializeAlertLog();
void UpdateAlertLog(const AlertSnapshot& snapshot, unsigned long now);
void RecordAlertEvent(AlertType type, uint8_t detail = 0);

// Copie les entrées, de la plus récente à la plus ancienne.
size_t GetAlertLogEntries(AlertLogEntry* entries, size_t maxEntries);
uint32_t GetAlertLogRevision();
// Dernière alerte principale terminée encore présente dans le journal.
bool GetLastEndedPrimaryAlert(AlertType& type);
// Supprime les entrées terminées et renvoie leur nombre.
size_t ClearEndedAlertLogEntries();

#endif
