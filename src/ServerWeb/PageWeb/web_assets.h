#pragma once
// Déclarations extern des ressources web (gzip-compressées pour HTML/CSS/JS)
#include <Arduino.h>

// Utilisation de la PSRAM pour les assets web (libère la SRAM)
#ifdef BOARD_HAS_PSRAM
  #define WEB_STORAGE_ATTR
#else
  #define WEB_STORAGE_ATTR PROGMEM
#endif

class AsyncWebServer;
void AddRoutesToServer(AsyncWebServer& server, const char* PortailPage, const char* DefaultPage);

namespace WEB {
  extern const uint8_t AdminSetup_html[] WEB_STORAGE_ATTR;
  extern const size_t AdminSetup_html_len;
  extern const uint8_t Alarm_html[] WEB_STORAGE_ATTR;
  extern const size_t Alarm_html_len;
  extern const uint8_t Capteur_html[] WEB_STORAGE_ATTR;
  extern const size_t Capteur_html_len;
  extern const uint8_t Capteur_js[] WEB_STORAGE_ATTR;
  extern const size_t Capteur_js_len;
  extern const uint8_t Cold_html[] WEB_STORAGE_ATTR;
  extern const size_t Cold_html_len;
  extern const uint8_t Dehum_html[] WEB_STORAGE_ATTR;
  extern const size_t Dehum_html_len;
  extern const uint8_t fan_html[] WEB_STORAGE_ATTR;
  extern const size_t fan_html_len;
  extern const uint8_t favicon_ico[] WEB_STORAGE_ATTR;
  extern const size_t favicon_ico_len;
  extern const uint8_t Heat_html[] WEB_STORAGE_ATTR;
  extern const size_t Heat_html_len;
  extern const uint8_t Hum_html[] WEB_STORAGE_ATTR;
  extern const size_t Hum_html_len;
  extern const uint8_t Menu_html[] WEB_STORAGE_ATTR;
  extern const size_t Menu_html_len;
  extern const uint8_t Menu_js[] WEB_STORAGE_ATTR;
  extern const size_t Menu_js_len;
  extern const uint8_t Mqtt_html[] WEB_STORAGE_ATTR;
  extern const size_t Mqtt_html_len;
  extern const uint8_t style_css[] WEB_STORAGE_ATTR;
  extern const size_t style_css_len;
  extern const uint8_t Wifi_html[] WEB_STORAGE_ATTR;
  extern const size_t Wifi_html_len;
} // namespace WEB
