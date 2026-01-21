
#ifndef WEBSERVER_H
#define WEBSERVER_H


#ifndef HTTP_GET
    #define HTTP_GET     (1 << 0)
    #define HTTP_POST    (1 << 1)
    #define HTTP_DELETE  (1 << 2)
    #define HTTP_PUT     (1 << 3)
    #define HTTP_PATCH   (1 << 4)
    #define HTTP_HEAD    (1 << 5)
    #define HTTP_OPTIONS (1 << 6)
    #define HTTP_ANY     (1 << 7)
#endif
#include <ESPAsyncWebServer.h>

void setupWebHandlers();

void handleRestart(AsyncWebServerRequest *request);


void handleScanWifi(AsyncWebServerRequest *request);
void handleScanResultWifi(AsyncWebServerRequest *request);
void handleSaveWiFi(AsyncWebServerRequest *request);





#endif
