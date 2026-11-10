#include "WebSocketApi.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstring>
#include <math.h>
#include <new>

#include <esp_heap_caps.h>

#include "Alarm/AlertLog.h"
#include "Alarm/SafetyAlarm.h"
#include "Controller/ConfigurationLimits.h"
#include "Controller/ControllerValidation.h"
#include "Data/ControllerData.h"
#include "Global/Global.h"
#include "ServerWeb/WsCommands.h"

using namespace wsapi;

namespace {

constexpr size_t MaximumCommandBytes = 1024;
constexpr size_t CommandCacheSize = 16;
constexpr size_t MaximumRequestIdLength = 64;
constexpr size_t StateFrameAllocationOverhead = 64;

struct WebSocketClientState {
    String incomingMessage;
    uint64_t expectedMessageLength = 0;
};

struct GlobalCachedCommand {
    bool valid = false;
    String id;
    uint64_t fingerprint = 0;
    String response;
};

GlobalCachedCommand commandCache[CommandCacheSize];
size_t nextCommandCacheEntry = 0;
SemaphoreHandle_t commandMutex = nullptr;
unsigned long lastStateSendWarning = 0;
// Positionné par la tâche réseau à la connexion d'un client; consommé par la boucle principale.
volatile bool stateRequested = false;


uint64_t fingerprintMessage(const String& message) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < message.length(); ++i) {
        hash ^= static_cast<uint8_t>(message[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void logStateSendWarning(const char* reason, size_t frameLength = 0,
                         size_t largestBlock = 0) {
    const unsigned long now = millis();
    if (now - lastStateSendWarning < 5000) {
        return;
    }
    lastStateSendWarning = now;
    Serial.printf(
        "[WS] Etat non envoye: %s (trame %u, bloc libre %u octets).\n",
        reason, static_cast<unsigned int>(frameLength),
        static_cast<unsigned int>(largestBlock));
}

// Sérialise l'état directement dans le tampon partagé que la bibliothèque
// envoie à tous les clients : une seule copie de la trame en mémoire.
AsyncWebSocketSharedBuffer buildStateFrame() {
    JsonDocument document;
    document["version"] = ProtocolVersion;
    document["type"] = "state";
    FillGlobalStatus(document["payload"].to<JsonObject>());
    if (document.overflowed()) {
        logStateSendWarning("document d'etat incomplet");
        return nullptr;
    }

    const size_t frameLength = measureJson(document);
    const size_t largestBlock =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (largestBlock < frameLength + StateFrameAllocationOverhead) {
        logStateSendWarning("fragmentation ou memoire insuffisante",
                            frameLength, largestBlock);
        return nullptr;
    }

    AsyncWebSocketSharedBuffer buffer;
    try {
        buffer = std::make_shared<std::vector<uint8_t>>(frameLength + 1);
    } catch (const std::bad_alloc&) {
        logStateSendWarning("allocation de trame impossible", frameLength,
                            largestBlock);
        return nullptr;
    }

    const size_t written =
        serializeJson(document, buffer->data(), buffer->size());
    if (written != frameLength) {
        logStateSendWarning("serialisation d'etat incomplete", frameLength);
        return nullptr;
    }
    buffer->resize(frameLength);
    return buffer;
}

bool clientReadyForState(const AsyncWebSocketClient& client) {
    // Une trame d'état n'est ajoutée qu'à une file vide, pour laisser la
    // place aux réponses de commandes et ne jamais empiler d'états périmés.
    return client.status() == WS_CONNECTED && client.queueLen() == 0;
}
}

namespace wsapi {

String makeResponse(const String& id, bool ok, const char* errorCode,
                    const String& errorMessage) {
    JsonDocument document;
    document["version"] = ProtocolVersion;
    document["type"] = "response";
    document["id"] = id;
    document["ok"] = ok;
    if (errorCode != nullptr) {
        document["error"]["code"] = errorCode;
        document["error"]["message"] = errorMessage;
    }

    String response;
    if (serializeJson(document, response) == 0) {
        Serial.println("[WS] Impossible de sérialiser une réponse.");
        return "";
    }
    return response;
}

String makeMutationResponse(const String& id, bool persisted,
                            bool restartRequired) {
    JsonDocument document;
    document["version"] = ProtocolVersion;
    document["type"] = "response";
    document["id"] = id;
    document["ok"] = persisted;
    JsonObject result = document["result"].to<JsonObject>();
    result["applied"] = true;
    result["persisted"] = persisted;
    if (restartRequired) {
        result["restartRequired"] = true;
    }
    if (!persisted) {
        document["error"]["code"] = "persistence_failed";
        document["error"]["message"] =
            "La valeur est appliquée en mémoire, mais sa sauvegarde NVS a échoué.";
    }

    String response;
    if (serializeJson(document, response) == 0) {
        Serial.println("[WS] Impossible de sérialiser le résultat de sauvegarde.");
        return "";
    }
    return response;
}

String makeAcceptedResponse(const String& id, const char* key,
                            const char* value) {
    JsonDocument document;
    document["version"] = ProtocolVersion;
    document["type"] = "response";
    document["id"] = id;
    document["ok"] = true;
    document["result"][key] = value;

    String response;
    if (serializeJson(document, response) == 0) {
        Serial.println("[WS] Impossible de sérialiser l'accusé de réception.");
        return "";
    }
    return response;
}

void sendResponse(AsyncWebSocketClient* client, const String& id,
                  const String& action, uint64_t fingerprint,
                  const String& response, bool cacheable) {
    if (response.isEmpty()) {
        return;
    }

    if (cacheable) {
        GlobalCachedCommand& entry = commandCache[nextCommandCacheEntry];
        entry.id = id;
        entry.fingerprint = fingerprint;
        entry.response = response;
        entry.valid = true;
        nextCommandCacheEntry =
            (nextCommandCacheEntry + 1) % CommandCacheSize;
    }

    if (!client->text(response)) {
        Serial.printf("[WS] Envoi de réponse impossible au client %u.\n",
                      client->id());
    }
}

void sendError(AsyncWebSocketClient* client, const String& id,
               const String& action, uint64_t fingerprint,
               const char* code, const String& message,
               bool cacheable) {
    sendResponse(client, id, action, fingerprint,
                 makeResponse(id, false, code, message), cacheable);
}

void sendReadResult(AsyncWebSocketClient* client, const String& id,
                    const String& action, uint64_t fingerprint,
                    JsonDocument& result) {
    JsonDocument responseDocument;
    responseDocument["version"] = ProtocolVersion;
    responseDocument["type"] = "response";
    responseDocument["id"] = id;
    responseDocument["ok"] = true;
    responseDocument["result"].set(result.as<JsonVariantConst>());

    String response;
    if (serializeJson(responseDocument, response) == 0) {
        Serial.println("[WS] Impossible de sérialiser le résultat de lecture.");
        return;
    }
    sendResponse(client, id, action, fingerprint, response, false);
}

bool readString(JsonObjectConst payload, const char* key, size_t maxLength,
                String& value) {
    const JsonVariantConst item = payload[key];
    if (!item.is<const char*>()) {
        return false;
    }
    value = item.as<String>();
    return value.length() <= maxLength;
}

bool readBoolean(JsonObjectConst payload, const char* key, bool& value) {
    const JsonVariantConst item = payload[key];
    if (!item.is<bool>()) {
        return false;
    }
    value = item.as<bool>();
    return true;
}

bool readFiniteFloat(JsonObjectConst payload, const char* key, float& value) {
    const JsonVariantConst item = payload[key];
    if (!item.is<float>()) {
        return false;
    }
    value = item.as<float>();
    return isfinite(value);
}

bool readBoundedUnsigned(JsonObjectConst payload, const char* key,
                         unsigned int minimum, unsigned int maximum,
                         unsigned int& value) {
    const JsonVariantConst item = payload[key];
    if (!item.is<unsigned int>()) {
        return false;
    }
    const unsigned int parsed = item.as<unsigned int>();
    if (parsed < minimum || parsed > maximum) {
        return false;
    }
    value = parsed;
    return true;
}

void sendEvent(const char* eventName, const String& requestId,
               JsonVariantConst payload) {
    JsonDocument eventDocument;
    eventDocument["version"] = ProtocolVersion;
    eventDocument["type"] = "event";
    eventDocument["event"] = eventName;
    eventDocument["requestId"] = requestId;
    eventDocument["payload"].set(payload);

    String frame;
    if (serializeJson(eventDocument, frame) == 0) {
        Serial.println("[WS] Impossible de sérialiser un événement.");
        return;
    }
    ws.textAll(frame);
}

}

namespace {

void handleCommand(AsyncWebSocketClient* client,
                  WebSocketClientState* clientState,
                  const String& message) {
    JsonDocument request;
    const DeserializationError parseError = deserializeJson(request, message);
    if (parseError) {
        sendError(client, "", "", 0, "invalid_json",
                  "Message JSON invalide.");
        return;
    }

    const JsonObjectConst requestObject = request.as<JsonObjectConst>();
    String id;
    String action;
    if (!requestObject["id"].is<const char*>() ||
        !requestObject["version"].is<int>() ||
        !readString(requestObject, "id", MaximumRequestIdLength, id) ||
        id.isEmpty()) {
        sendError(client, "", "", 0, "invalid_envelope",
                  "La version et l'identifiant de requête sont obligatoires.");
        return;
    }
    if (requestObject["version"].as<int>() != ProtocolVersion) {
        sendError(client, id, "", 0, "unsupported_version",
                  "Version de protocole non prise en charge.");
        return;
    }
    if (!readString(requestObject, "action", 48, action) ||
        action.isEmpty() ||
        !requestObject["payload"].is<JsonObjectConst>()) {
        sendError(client, id, action, 0, "invalid_envelope",
                  "L'action et l'objet payload sont obligatoires.");
        return;
    }

    const uint64_t fingerprint = fingerprintMessage(message);
    for (size_t i = 0; i < CommandCacheSize; ++i) {
        if (commandCache[i].valid && commandCache[i].id == id) {
            if (commandCache[i].fingerprint == fingerprint) {
                client->text(commandCache[i].response);
            } else {
                sendError(client, id, action, fingerprint, "id_conflict",
                          "Cet identifiant a déjà été utilisé pour une autre commande.");
            }
            return;
        }
    }

    const WsCommand cmd = {
        client, id, action, fingerprint,
        requestObject["payload"].as<JsonObjectConst>()
    };
    const struct {
        const WsAction* actions;
        size_t count;
    } tables[] = {
        {SettingsActions, SettingsActionCount},
        {AlertActions, AlertActionCount},
        {NetworkActions, NetworkActionCount},
    };
    for (const auto& table : tables) {
        for (size_t i = 0; i < table.count; ++i) {
            if (action == table.actions[i].name) {
                table.actions[i].handler(cmd);
                return;
            }
        }
    }

    sendError(client, id, action, fingerprint, "unknown_action",
              "Action WebSocket inconnue.");
}
}

bool InitializeWebSocketApi() {
    if (commandMutex == nullptr) {
        commandMutex = xSemaphoreCreateMutex();
    }
    if (commandMutex == nullptr) {
        Serial.println("[WS] Impossible de créer le mutex de commandes.");
        return false;
    }
    return true;
}

void HandleWebSocketApiEvent(AsyncWebSocket* server,
                             AsyncWebSocketClient* client,
                             AwsEventType type, void* arg,
                             uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        client->_tempObject = new WebSocketClientState();
        if (client->_tempObject == nullptr) {
            Serial.printf("[WS] Mémoire insuffisante pour le client %u.\n",
                          client->id());
            client->close(1011, "mémoire insuffisante");
            return;
        }
        // L'état est construit par la boucle principale au prochain cycle,
        // pas dans la tâche réseau.
        stateRequested = true;
        return;
    }

    if (type == WS_EVT_DISCONNECT) {
        delete static_cast<WebSocketClientState*>(client->_tempObject);
        client->_tempObject = nullptr;
        return;
    }

    if (type != WS_EVT_DATA || arg == nullptr || data == nullptr) {
        return;
    }

    if (commandMutex == nullptr ||
        xSemaphoreTake(commandMutex, portMAX_DELAY) != pdTRUE) {
        Serial.println("[WS] Impossible de verrouiller le traitement des commandes.");
        return;
    }

    WebSocketClientState* state =
        static_cast<WebSocketClientState*>(client->_tempObject);
    AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
    // ESPAsyncWebServer ne renseigne pas message_opcode quand une trame
    // arrive en un seul paquet TCP : on se base sur l'opcode de la trame.
    // Les messages fragmentés en plusieurs trames ne sont pas acceptés.
    if (state == nullptr || info->opcode != WS_TEXT || info->num != 0 ||
        info->len > MaximumCommandBytes ||
        info->index != state->incomingMessage.length()) {
        if (state != nullptr) {
            state->incomingMessage = "";
            state->expectedMessageLength = 0;
        }
        sendError(client, "", "", 0, "invalid_message",
                  "Message WebSocket invalide ou trop volumineux.");
        xSemaphoreGive(commandMutex);
        return;
    }

    if (info->index == 0) {
        state->incomingMessage = "";
        state->expectedMessageLength = info->len;
    }
    if (state->expectedMessageLength != info->len ||
        !state->incomingMessage.concat(
            reinterpret_cast<const char*>(data), len)) {
        state->incomingMessage = "";
        state->expectedMessageLength = 0;
        sendError(client, "", "", 0, "invalid_message",
                  "Message WebSocket fragmenté de manière invalide.");
        xSemaphoreGive(commandMutex);
        return;
    }

    if (info->final && state->incomingMessage.length() == info->len) {
        handleCommand(client, state, state->incomingMessage);
        state->incomingMessage = "";
        state->expectedMessageLength = 0;
    }
    xSemaphoreGive(commandMutex);
}

void ProcessWebSocketApi() {
    if (commandMutex == nullptr ||
        xSemaphoreTake(commandMutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    ProcessNetworkCommands();
    xSemaphoreGive(commandMutex);
}

bool TakeWebSocketStateRequest() {
    const bool requested = stateRequested;
    stateRequested = false;
    return requested;
}

void SendWebSocketStateToAll() {
    bool anyClientReady = false;
    for (const AsyncWebSocketClient& client : ws.getClients()) {
        anyClientReady = anyClientReady || clientReadyForState(client);
    }
    if (!anyClientReady) {
        return;
    }

    const AsyncWebSocketSharedBuffer frame = buildStateFrame();
    if (!frame) {
        return;
    }

    for (AsyncWebSocketClient& client : ws.getClients()) {
        if (!clientReadyForState(client)) {
            continue;
        }
        try {
            client.text(frame);
        } catch (const std::bad_alloc&) {
            logStateSendWarning("file d'envoi saturee", frame->size());
            return;
        }
    }
}
