#pragma once
// Déclarations extern des ressources web
#include <Arduino.h>

class AsyncWebServer;
void AddRoutesToServer(AsyncWebServer& server, const char* DefaultPage);

namespace WEB {
  extern const uint8_t favicon_ico[] PROGMEM;
  extern const size_t favicon_ico_len;
  extern const char style_css[] PROGMEM;
  extern const size_t style_css_len;
  extern const char Wifi_html[] PROGMEM;
  extern const size_t Wifi_html_len;
  extern const char Wifi_js[] PROGMEM;
  extern const size_t Wifi_js_len;
} // namespace WEB
