#ifndef WEBSOCKET_API_H
#define WEBSOCKET_API_H

#include <ESPAsyncWebServer.h>

bool InitializeWebSocketApi();
void HandleWebSocketApiEvent(AsyncWebSocket *server,
                             AsyncWebSocketClient *client,
                             AwsEventType type, void *arg,
                             uint8_t *data, size_t len);
void ProcessWebSocketApi();
bool TakeWebSocketStateRequest();
void SendWebSocketStateToAll();

#endif
