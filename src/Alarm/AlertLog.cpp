#include "Alarm/AlertLog.h"

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "Alarm/AlertManager.h"

namespace {

// Une alerte qui réapparaît dans ce délai après sa fin réutilise la même
// entrée (compteur d'occurrences) au lieu d'en créer une nouvelle.
constexpr uint32_t RepeatMergeWindowMs = 5UL * 60UL * 1000UL;
// Au démarrage, Wi-Fi, MQTT et mesures ne sont pas encore disponibles :
// une alerte n'est journalisée que si elle dure au-delà de ce délai.
constexpr uint32_t StartupGraceMs = 60UL * 1000UL;
constexpr size_t ContinuousAlertTypeCount =
    static_cast<size_t>(AlertType::AbnormalRestart);

AlertLogEntry entries[AlertLogCapacity];
size_t entryCount = 0;
uint32_t revision = 0;
bool previouslyActive[ContinuousAlertTypeCount] = {};
SemaphoreHandle_t logMutex = nullptr;

class LogLock {
public:
    LogLock() : locked_(logMutex != nullptr &&
                        xSemaphoreTake(logMutex, portMAX_DELAY) == pdTRUE) {}
    ~LogLock() {
        if (locked_) {
            xSemaphoreGive(logMutex);
        }
    }
    bool locked() const { return locked_; }

private:
    bool locked_;
};

uint32_t lastActivityAgeMs(const AlertLogEntry& entry, uint32_t now) {
    return entry.Active ? 0 : now - entry.LastChangeMs;
}

void removeEntryAt(size_t index) {
    for (size_t i = index + 1; i < entryCount; ++i) {
        entries[i - 1] = entries[i];
    }
    --entryCount;
}

// Libère une place en supprimant l'entrée terminée la plus ancienne
// (ou la plus ancienne tout court si toutes sont en cours).
void makeRoom(uint32_t now) {
    if (entryCount < AlertLogCapacity) {
        return;
    }
    size_t victim = 0;
    bool foundEnded = false;
    uint32_t oldestAge = 0;
    for (size_t i = 0; i < entryCount; ++i) {
        if (entries[i].Active) {
            continue;
        }
        const uint32_t age = lastActivityAgeMs(entries[i], now);
        if (!foundEnded || age > oldestAge) {
            victim = i;
            oldestAge = age;
            foundEnded = true;
        }
    }
    removeEntryAt(victim);
}

AlertLogEntry* findMergeableEntry(AlertType type, uint32_t now) {
    for (size_t i = entryCount; i-- > 0;) {
        AlertLogEntry& entry = entries[i];
        if (entry.Type != type) {
            continue;
        }
        if (!entry.Active && now - entry.LastChangeMs < RepeatMergeWindowMs) {
            return &entry;
        }
        return nullptr;
    }
    return nullptr;
}

void openEntry(AlertType type, uint32_t now, bool pointEvent, uint8_t detail) {
    AlertLogEntry* entry = findMergeableEntry(type, now);
    if (entry != nullptr) {
        if (entry->Occurrences < UINT8_MAX) {
            ++entry->Occurrences;
        }
        entry->Active = !pointEvent;
        entry->LastChangeMs = now;
        entry->Detail = detail;
    } else {
        makeRoom(now);
        entries[entryCount++] = {type, !pointEvent, 1, now, now, detail};
    }
    ++revision;
}

void closeEntry(AlertType type, uint32_t now) {
    for (size_t i = entryCount; i-- > 0;) {
        if (entries[i].Type == type && entries[i].Active) {
            entries[i].Active = false;
            entries[i].LastChangeMs = now;
            ++revision;
            return;
        }
    }
}

bool isAbnormalReset(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}

}

const char* GetAlertTypeCode(AlertType type) {
    switch (type) {
        case AlertType::NanoLinkLost: return "nano_link_lost";
        case AlertType::MeasurementsUnavailable: return "measurements_unavailable";
        case AlertType::HighTemperature: return "high_temperature";
        case AlertType::NanoFallback: return "nano_fallback";
        case AlertType::WifiDisconnected: return "wifi_disconnected";
        case AlertType::MqttDisconnected: return "mqtt_disconnected";
        case AlertType::AbnormalRestart: return "abnormal_restart";
        case AlertType::SaveFailed: return "save_failed";
        default: return "unknown";
    }
}

const char* GetAlertTypeLabel(AlertType type) {
    switch (type) {
        case AlertType::NanoLinkLost: return "Liaison avec le Nano perdue";
        case AlertType::MeasurementsUnavailable: return "Mesures indisponibles";
        case AlertType::HighTemperature: return "Temperature trop elevee";
        case AlertType::NanoFallback: return "Mode secours du Nano";
        case AlertType::WifiDisconnected: return "Wi-Fi indisponible";
        case AlertType::MqttDisconnected: return "MQTT indisponible";
        case AlertType::AbnormalRestart: return "Redemarrage anormal";
        case AlertType::SaveFailed: return "Echec de sauvegarde";
        default: return "Alerte inconnue";
    }
}

bool IsPrimaryAlertType(AlertType type) {
    return type == AlertType::NanoLinkLost ||
           type == AlertType::MeasurementsUnavailable ||
           type == AlertType::HighTemperature ||
           type == AlertType::NanoFallback;
}

size_t GetActiveAlertTypes(const AlertSnapshot& snapshot, AlertType* types,
                           size_t maxTypes) {
    const bool active[ContinuousAlertTypeCount] = {
        snapshot.NanoLinkLost,
        snapshot.MeasurementsUnavailable,
        snapshot.HighTemperatureActive,
        snapshot.NanoFallback,
        snapshot.WifiDisconnected,
        snapshot.MqttDisconnected,
    };
    size_t count = 0;
    for (size_t i = 0; i < ContinuousAlertTypeCount && count < maxTypes; ++i) {
        if (active[i]) {
            types[count++] = static_cast<AlertType>(i);
        }
    }
    return count;
}

void InitializeAlertLog() {
    if (logMutex == nullptr) {
        logMutex = xSemaphoreCreateMutex();
    }
    const esp_reset_reason_t reason = esp_reset_reason();
    if (isAbnormalReset(reason)) {
        RecordAlertEvent(AlertType::AbnormalRestart, static_cast<uint8_t>(reason));
        Serial.printf("[ALERTE] Redémarrage anormal (cause %d)\n",
                      static_cast<int>(reason));
    }
}

void UpdateAlertLog(const AlertSnapshot& snapshot, unsigned long now) {
    if (now < StartupGraceMs) {
        return;
    }
    AlertType activeTypes[ContinuousAlertTypeCount];
    const size_t activeCount =
        GetActiveAlertTypes(snapshot, activeTypes, ContinuousAlertTypeCount);
    bool active[ContinuousAlertTypeCount] = {};
    for (size_t i = 0; i < activeCount; ++i) {
        active[static_cast<size_t>(activeTypes[i])] = true;
    }

    LogLock lock;
    if (!lock.locked()) {
        return;
    }
    for (size_t i = 0; i < ContinuousAlertTypeCount; ++i) {
        if (active[i] == previouslyActive[i]) {
            continue;
        }
        const AlertType type = static_cast<AlertType>(i);
        if (active[i]) {
            openEntry(type, now, false, 0);
        } else {
            closeEntry(type, now);
        }
        previouslyActive[i] = active[i];
    }
}

void RecordAlertEvent(AlertType type, uint8_t detail) {
    LogLock lock;
    if (!lock.locked()) {
        return;
    }
    openEntry(type, millis(), true, detail);
}

size_t GetAlertLogEntries(AlertLogEntry* output, size_t maxEntries) {
    LogLock lock;
    if (!lock.locked()) {
        return 0;
    }
    const uint32_t now = millis();
    const size_t count = entryCount < maxEntries ? entryCount : maxEntries;
    size_t written = 0;
    for (size_t i = entryCount; i-- > 0 && written < count;) {
        output[written++] = entries[i];
    }
    // Tri par activité la plus récente : alertes en cours d'abord.
    for (size_t i = 1; i < written; ++i) {
        const AlertLogEntry current = output[i];
        size_t j = i;
        while (j > 0 && lastActivityAgeMs(output[j - 1], now) >
                            lastActivityAgeMs(current, now)) {
            output[j] = output[j - 1];
            --j;
        }
        output[j] = current;
    }
    return written;
}

uint32_t GetAlertLogRevision() {
    LogLock lock;
    return revision;
}

bool GetLastEndedPrimaryAlert(AlertType& type) {
    AlertLogEntry snapshot[AlertLogCapacity];
    const size_t count = GetAlertLogEntries(snapshot, AlertLogCapacity);
    for (size_t i = 0; i < count; ++i) {
        if (!snapshot[i].Active && IsPrimaryAlertType(snapshot[i].Type)) {
            type = snapshot[i].Type;
            return true;
        }
    }
    return false;
}

size_t ClearEndedAlertLogEntries() {
    LogLock lock;
    if (!lock.locked()) {
        return 0;
    }
    size_t removed = 0;
    for (size_t i = entryCount; i-- > 0;) {
        if (!entries[i].Active) {
            removeEntryAt(i);
            ++removed;
        }
    }
    if (removed > 0) {
        ++revision;
    }
    return removed;
}
