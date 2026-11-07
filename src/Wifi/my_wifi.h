#ifndef MY_WIFI_H
#define  MY_WIFI_H

#include <Arduino.h>

extern String APSsid;
extern String APPass;

bool InitializeWifiIdentity();
void ConnectToWifi();
void StartAccessPoint();
void Gestion_DNS_mDNS();
void Set_WebSocket();
void SendValueWebSocket();

#endif