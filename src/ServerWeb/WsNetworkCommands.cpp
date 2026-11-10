#include "ServerWeb/WsCommands.h"

#include <WiFi.h>
#include "Controller/ConfigurationLimits.h"
#include "Data/ControllerData.h"

namespace wsapi {

namespace {

wifi_mode_t modeBeforeScan = WIFI_OFF;
constexpr unsigned long WifiScanTimeoutMs = 15000;
unsigned long scanStartedAt = 0;
String activeScanRequestId;
String completedScanResults;
bool restartPending = false;
unsigned long restartAt = 0;

void handleGetWifiScanResults(const WsCommand& cmd) {
    JsonDocument result;
    result["scanning"] = Get_WifiScanning();
    result["hasResults"] = !completedScanResults.isEmpty();
    if (!completedScanResults.isEmpty()) {
        JsonDocument scanResult;
        if (deserializeJson(scanResult, completedScanResults)) {
            sendError(cmd, "scan_results_unavailable",
                      "Les résultats du scan ne sont pas disponibles.");
            return;
        }
        result["networks"].set(scanResult["networks"]);
    } else {
        result["networks"].to<JsonArray>();
    }
    sendReadResult(cmd, result);
    return;
}

void handleScanWifi(const WsCommand& cmd) {
    if (Get_WifiScanning()) {
        sendError(cmd, "operation_in_progress",
                  "Un scan Wi-Fi est déjà en cours.", true);
        return;
    }

    modeBeforeScan = WiFi.getMode();
    activeScanRequestId = cmd.id;
    completedScanResults = "";
    const int scanStart = WiFi.scanNetworks(true);
    if (scanStart == -2) {
        activeScanRequestId = "";
        sendError(cmd, "scan_start_failed",
                  "Impossible de démarrer le scan Wi-Fi.", true);
        return;
    }
    Set_WifiScanning(true);
    scanStartedAt = millis();
    sendResponse(cmd,
                 makeAcceptedResponse(cmd.id, "accepted", "scanStarted"), true);
    return;
}

void handleSaveWifiSettings(const WsCommand& cmd) {
    String ssid;
    String password;
    if (!readString(cmd.payload, "ssid", 32, ssid) || ssid.isEmpty() ||
        !readString(cmd.payload, "password", 64, password)) {
        sendError(cmd, "invalid_parameters",
                  "Le nom du réseau ou le mot de passe Wi-Fi est invalide.", true);
        return;
    }
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataWifi.WifiSsid = ssid;
    DataWifi.WifiPass = password;
    xSemaphoreGive(DataMutex);
    const bool persisted = Save_Wifi_Parameters();
    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted, persisted), true);
    return;
}

void handleSaveMqttSettings(const WsCommand& cmd) {
    bool enabled;
    unsigned int port;
    String server;
    String user;
    String name;
    String password;
    bool clearPassword = false;
    if (!readBoolean(cmd.payload, "enabled", enabled) ||
        !readString(cmd.payload, "server", 128, server) ||
        !readBoundedUnsigned(cmd.payload, "port", MinMqttPort, MaxMqttPort, port) ||
        !readString(cmd.payload, "user", 128, user) ||
        !readString(cmd.payload, "deviceName", 128, name)) {
        sendError(cmd, "invalid_parameters",
                  "Paramètres MQTT manquants ou invalides.", true);
        return;
    }
    if ((enabled && server.isEmpty()) || server.indexOf("://") >= 0) {
        sendError(cmd, "invalid_mqtt_host",
                  "Saisissez un nom d'hôte ou une adresse IP, sans protocole.");
        return;
    }
    if (cmd.payload["password"].is<const char*>()) {
        password = cmd.payload["password"].as<String>();
        if (password.length() > 128) {
            sendError(cmd, "invalid_parameters",
                      "Le mot de passe MQTT est trop long.", true);
            return;
        }
    }
    if (!cmd.payload["clearPassword"].isNull() &&
        !readBoolean(cmd.payload, "clearPassword", clearPassword)) {
        sendError(cmd, "invalid_parameters",
                  "L'cmd.action sur le mot de passe MQTT est invalide.", true);
        return;
    }
    if (clearPassword && !password.isEmpty()) {
        sendError(cmd, "invalid_parameters",
                  "La suppression du mot de passe ne peut pas être combinée à un nouveau mot de passe.", true);
        return;
    }

    xSemaphoreTake(DataMutex, portMAX_DELAY);
    DataMqtt.MqttEnable = enabled;
    DataMqtt.MqttServer = server;
    DataMqtt.MqttPort = static_cast<uint16_t>(port);
    DataMqtt.MqttUser = user;
    DataMqtt.MqttDeviceName = name;
    if (clearPassword) {
        DataMqtt.MqttPassword = "";
    } else if (!password.isEmpty()) {
        DataMqtt.MqttPassword = password;
    }
    xSemaphoreGive(DataMutex);
    const bool persisted = Save_Mqtt_Parameters();
    sendResponse(cmd,
                 makeMutationResponse(cmd.id, persisted, persisted), true);
    return;
}

void handleRestartDevice(const WsCommand& cmd) {
    if (restartPending) {
        sendError(cmd, "restart_in_progress",
                  "Un redémarrage est déjà planifié.", true);
        return;
    }
    const String response =
        makeAcceptedResponse(cmd.id, "accepted", "restartScheduled");
    sendResponse(cmd, response, true);
    restartPending = true;
    restartAt = millis() + 750;
    return;
}

void processWifiScan() {
    if (!Get_WifiScanning()) {
        return;
    }

    const int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) {
        return;
    }
    // Le framework déclare le scan échoué après 6 s, alors qu'en mode station
    // connectée le scan dure plus longtemps et se termine bien ensuite.
    if (count == WIFI_SCAN_FAILED &&
        millis() - scanStartedAt < WifiScanTimeoutMs) {
        return;
    }

    const bool scanFailed = count == WIFI_SCAN_FAILED;
    JsonDocument scanDocument;
    JsonArray networks = scanDocument["networks"].to<JsonArray>();
    if (!scanFailed) {
        for (int i = 0; i < count; ++i) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(i);
            network["rssi"] = WiFi.RSSI(i);
            network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
    }

    String resultJson;
    if (serializeJson(scanDocument, resultJson) == 0) {
        Serial.println("[WS] Impossible de sérialiser les résultats du scan Wi-Fi.");
    }

    bool modeRestored = true;
    if (modeBeforeScan != WIFI_OFF) {
        modeRestored = WiFi.mode(modeBeforeScan);
        if (!modeRestored) {
            Serial.println("[WS] Impossible de restaurer le mode Wi-Fi après le scan.");
        }
    }

    WiFi.scanDelete();
    Set_WifiScanning(false);
    const String requestId = activeScanRequestId;
    activeScanRequestId = "";

    if (!scanFailed && !resultJson.isEmpty()) {
        completedScanResults = resultJson;
        scanDocument["modeRestored"] = modeRestored;
        sendEvent("wifiScanComplete", requestId, scanDocument.as<JsonVariantConst>());
        return;
    }

    JsonDocument failurePayload;
    failurePayload["message"] = scanFailed
        ? "Le scan Wi-Fi a échoué."
        : "Impossible de préparer les résultats du scan Wi-Fi.";
    failurePayload["modeRestored"] = modeRestored;
    sendEvent("wifiScanFailed", requestId, failurePayload.as<JsonVariantConst>());
}

}

const WsAction NetworkActions[] = {
    {"getWifiScanResults", handleGetWifiScanResults},
    {"scanWifi", handleScanWifi},
    {"saveWifiSettings", handleSaveWifiSettings},
    {"saveMqttSettings", handleSaveMqttSettings},
    {"restartDevice", handleRestartDevice},
};
const size_t NetworkActionCount =
    sizeof(NetworkActions) / sizeof(NetworkActions[0]);


void ProcessNetworkCommands() {
    processWifiScan();
    if (restartPending &&
        static_cast<long>(millis() - restartAt) >= 0) {
        Serial.println("[WS] Redémarrage demandé par un client authentifié.");
        ESP.restart();
    }
}

}
