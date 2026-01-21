#ifndef MY_WIFI_H
#define  MY_WIFI_H

#include <Arduino.h>


extern String WifiSsid;
extern String WifiPass;
extern String APSsid ;
extern String APPass ;


void Get_WIFI_SsidAndPass(String &Ssid, String &pass);
void Save_WIFI_SsidAndPass(const String &Ssid, const String &pass);


void ConnectToWifi();



void Gestion_DNS_mDNS();

void InitWebSocket();

void Check_LinkStatus(const unsigned long now);

#endif