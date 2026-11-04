#ifndef WEB_SECURITY_H
#define WEB_SECURITY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

bool InitializeWebSecurity(String& apPassword);
bool HasWebAdminCredential();
bool SaveWebAdminCredential(const String& password);
bool ResetWebAdminCredential();
bool IsWebAdminSetupRequestAllowed(AsyncWebServerRequest* request);
void AddWebSecurityMiddleware(AsyncWebServer& server);

#endif
