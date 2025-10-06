#include "../GUI.h"

#ifdef GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
 #include GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
#endif


void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
}


void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    lv_obj_set_style_opa( GUI_Image__screen__fanspeed0, 0, 0 );  //Set_Opacity
    lv_obj_set_style_opa( GUI_Image__screen__fanspeed1, 0, 0 );  //Set_Opacity
    lv_obj_set_style_opa( GUI_Image__screen__fanspeed2, 0, 0 );  //Set_Opacity
    lv_obj_set_style_opa( GUI_Image__screen__fanspeed3, 0, 0 );  //Set_Opacity
    lv_obj_set_style_opa( GUI_Image__screen__fanspeedboost, 100, 0 );  //Set_Opacity
}


void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
}


