#include "LVGL/Lvgl_Global.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Global/Global.h"



void set_switch_state(lv_obj_t* sw, bool on) {
    if(on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else   lv_obj_clear_state(sw, LV_STATE_CHECKED);
}


//timer pour refresh le temps du chrono
void CallBackTimer_ScreenRefresh(lv_timer_t * Timer_ScreenRefresh){
    if(UiReady){
        if (lv_scr_act() == ui_ScreenGlobal) {
            Refresh_ScreenGlobal();
        }        
    }
}