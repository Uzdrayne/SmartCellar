#include <Arduino.h>
#include <lvgl.h>
#include "DisplayAndTouch/ConfigDisplay.h"
#include <ui.h>

#include "Wifi/my_wifi.h"
#include "Global/Global.h"

//Serial.printf("SRAM free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
 
//#define SPI SPI__DANGER_BLOCKED

void setup()
{

  UiReady=false;
  IsWsConnected = false;
  LinkStatus =false;

  my_TouchScreen_Init();
  tft.setBrightness(10);

  delay(5000);

  Serial.begin(115200); /* prepare for possible serial debug */
  Serial.println("Hello !");


  ui_init();
  tft.setBrightness(ScreenUserBrightness);

  InitControllerData(DataController);


  UiReady=true;

  ConnectToWifi(); 

}



void loop() {

  unsigned long now = millis();

  //si on est entrain d'essayer de se connecter
  if (TryingWifi) { 
    //si la connexion STA est réussite
    if (WifiSTA_OK) {
      TryingWifi = false; 
      if (lv_scr_act() != ui_ScreenGlobal){
        _ui_screen_change(&ui_ScreenGlobal, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGlobal_screen_init);
      }
    }
    if(WifiSsid==""){
      Serial.println("pas de Ssid enregistré");
      TryingWifi=false;
      WiFi.disconnect();
      if (lv_scr_act() != ui_ScreenWifi){
        _ui_screen_change(&ui_ScreenWifi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenWifi_screen_init);
      }  
    }
    //si toujours pas de connexion STA
    else if (now - WifiAttemptStart > 60000) {
      WifiAttemptStart=now;
      Serial.println("Timeout WiFi");
      TryingWifi=false;
      WiFi.disconnect();
      if (lv_scr_act() != ui_ScreenWifi){
        _ui_screen_change(&ui_ScreenWifi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenWifi_screen_init);
      }
    }
  }



  // 1. Gérer le réseau en priorité
  if (WifiSTA_OK) {
    webSocket.loop();
    Check_LinkStatus(now);
  } 
  else {if (dnsActive) dns.processNextRequest(); }


  // 2. Gérer l'affichage (seulement si le réseau n'est pas saturé)
  lv_timer_handler(); 


  // Gérer l'écran (auto luminosité)
  Handle_Screen_Brightness(now);


  delay(2); // Un délai plus court pour plus de réactivité réseau
}





