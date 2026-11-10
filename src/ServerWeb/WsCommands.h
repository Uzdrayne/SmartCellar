#ifndef WS_COMMANDS_H
#define WS_COMMANDS_H

// Interne à l'API WebSocket : enveloppe des commandes, utilitaires de réponse
// et tables d'actions par domaine (réglages, alertes, réseau).

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

namespace wsapi {

constexpr int ProtocolVersion = 1;

struct WsCommand {
    AsyncWebSocketClient* client;
    const String& id;
    const String& action;
    uint64_t fingerprint;
    JsonObjectConst payload;
};

using WsHandler = void (*)(const WsCommand&);

struct WsAction {
    const char* name;
    WsHandler handler;
};

String makeResponse(const String& id, bool ok, const char* errorCode = nullptr,
                    const String& errorMessage = String());
String makeMutationResponse(const String& id, bool persisted,
                            bool restartRequired = false);
String makeAcceptedResponse(const String& id, const char* key, const char* value);

// Les réponses « cacheable » sont rejouées si le même identifiant revient.
void sendResponse(AsyncWebSocketClient* client, const String& id,
                  const String& action, uint64_t fingerprint,
                  const String& response, bool cacheable);
void sendError(AsyncWebSocketClient* client, const String& id,
               const String& action, uint64_t fingerprint,
               const char* code, const String& message, bool cacheable = false);
void sendReadResult(AsyncWebSocketClient* client, const String& id,
                    const String& action, uint64_t fingerprint,
                    JsonDocument& result);
void sendEvent(const char* eventName, const String& requestId,
               JsonVariantConst payload);

inline void sendResponse(const WsCommand& cmd, const String& response, bool cacheable) {
    sendResponse(cmd.client, cmd.id, cmd.action, cmd.fingerprint, response, cacheable);
}
inline void sendError(const WsCommand& cmd, const char* code, const String& message,
                      bool cacheable = false) {
    sendError(cmd.client, cmd.id, cmd.action, cmd.fingerprint, code, message, cacheable);
}
inline void sendReadResult(const WsCommand& cmd, JsonDocument& result) {
    sendReadResult(cmd.client, cmd.id, cmd.action, cmd.fingerprint, result);
}

bool readString(JsonObjectConst payload, const char* key, size_t maxLength,
                String& value);
bool readBoolean(JsonObjectConst payload, const char* key, bool& value);
bool readFiniteFloat(JsonObjectConst payload, const char* key, float& value);
bool readBoundedUnsigned(JsonObjectConst payload, const char* key,
                         unsigned int minimum, unsigned int maximum,
                         unsigned int& value);

extern const WsAction SettingsActions[];
extern const size_t SettingsActionCount;
extern const WsAction AlertActions[];
extern const size_t AlertActionCount;
extern const WsAction NetworkActions[];
extern const size_t NetworkActionCount;

// Suivi du scan Wi-Fi asynchrone et du redémarrage différé (boucle principale).
void ProcessNetworkCommands();

}

#endif
