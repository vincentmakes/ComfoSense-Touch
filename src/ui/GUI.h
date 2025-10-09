#ifndef _GUI_HEADER_INCLUDED
#define _GUI_HEADER_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


#include "lvgl.h"
#include "ui_helpers.h"

extern lv_obj_t* GUI_Screen__screen;
extern lv_obj_t*  GUI_Panel__screen__panel;
LV_FONT_DECLARE( font_1 );
extern lv_obj_t*   GUI_Label__screen__time;
LV_FONT_DECLARE( defaultsmall_1 );
extern lv_obj_t*   GUI_Label__screen__date;
extern lv_obj_t*   GUI_Label__screen__filter;
extern lv_obj_t*   GUI_Image__screen__image;
extern lv_obj_t*   GUI_Image__screen__wifi;
extern lv_obj_t*  GUI_Panel__screen__panel_1;
extern lv_obj_t*   GUI_Button__screen__BSpeedMinus;
extern lv_obj_t*    GUI_Image__screen__minus;
extern lv_obj_t*   GUI_Button__screen__BSpeedPlus;
extern lv_obj_t*    GUI_Image__screen__plus;
extern lv_obj_t*   GUI_Button__screen__buttonspeedboost;
LV_FONT_DECLARE( largebold_1 );
extern lv_obj_t*    GUI_Label__screen__labelboost;
extern lv_obj_t*  GUI_Label__screen__insideTemp;
extern lv_obj_t*  GUI_Label__screen__outsideTemp;
extern lv_obj_t*  GUI_Label__screen__insideHum;
extern lv_obj_t*  GUI_Label__screen__outsideHum;
extern lv_obj_t*  GUI_Label__screen__auto;
extern lv_obj_t*  GUI_Image__screen__fanspeed0;
extern lv_obj_t*  GUI_Image__screen__fanspeed1;
extern lv_obj_t*  GUI_Image__screen__fanspeed2;
extern lv_obj_t*  GUI_Image__screen__fanspeed3;
extern lv_obj_t*  GUI_Image__screen__fanspeedboost;
extern lv_obj_t*  GUI_Dropdown__screen__modedropdown;
extern lv_obj_t*  GUI_Button__screen__centerfan;


// Screen-specific function declarations
void GUI_initScreen__screen ();
void GUI_initScreenTexts__screen ();
void GUI_initScreenStyles__screen ();

extern lv_style_t GUI_Style__class_IjzeXjGNl2hqzr__;
extern lv_style_t GUI_Style__class_YGYDXpOgt1PZ82__;
extern lv_style_t GUI_Style__class_Togbz3UaTq5x9x__;
extern lv_style_t GUI_Style__class_cGz9MAbjN4QZbT__;
extern lv_style_t GUI_Style__class_x3c54mLQde9cTS__;
extern lv_style_t GUI_Style__class_2QBFIVRjDIrU4g__;
extern lv_style_t GUI_Style__class_5up8hpXSh7ZNGY__;
extern lv_style_t GUI_Style__class_1v6NR7CCKSOY1p__;
extern lv_style_t GUI_Style__class_ZQFNgZyvWmNGIo__;
extern lv_style_t GUI_Style__class_qitjx2mC7B2BMR__;
extern lv_style_t GUI_Style__class_MeHq32ICIpTQi8__;
extern lv_style_t GUI_Style__class_fP8ERHUGv3DkcB__;
extern lv_style_t GUI_Style__class_LoaGBPa9UrG7CJ__;
extern lv_style_t GUI_Style__class_xEEcgMbVy8vNcO__;
extern lv_style_t GUI_Style__class_bAPqNI9bRId5Fl__;
extern lv_style_t GUI_Style__class_UtNSkp6cFAlBRt__;
extern lv_style_t GUI_Style__class_8RAAmaM7qkaMFY__;
extern lv_style_t GUI_Style__class_i65QyXalE1u3QM__;
extern lv_style_t GUI_Style__class_5qfkyxJclqQGgq__;
extern lv_style_t GUI_Style__class_e43nEdwxt3dT0L__;
extern lv_style_t GUI_Style__class_kr8vNLKWRCqnOJ__;
extern lv_style_t GUI_Style__class_V1ohqLxZAM4F0a__;


void GUI_load ();

void GUI_init ();

void GUI_refresh ();


void GUI_initHAL ();
void HAL_init ();

void GUI_initFramework ();

void GUI_loadContent ();


void GUI_initContent ();


void GUI_initTheme ();


void GUI_initScreens ();


void GUI_loadFirstScreen ();


void GUI_initScreenContents ();

void GUI_initScreenTexts ();

void GUI_initScreenStyles ();


void GUI_initGlobalStyles ();


void GUI_initAnimations ();


void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event);
void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event);
void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event);
void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event);

// Custom initialization functions
void GUI_init_fan_speed_display(void);
void GUI_init_dropdown_styling(void);

// Custom initialization functions
void GUI_init_fan_speed_display(void);
void GUI_init_dropdown_styling(void);
void GUI_request_display_refresh(void);
void GUI_process_display_refresh(void);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //_GUI_HEADER_INCLUDED

LV_IMG_DECLARE( upload_minus_22b4150ed8454f4f8d0531614cfc727c_png );
LV_IMG_DECLARE( upload_warningoutline_2343dccdd2da484cab2db0b1a6f4ab6b_png );
LV_IMG_DECLARE( upload_plus_68d2c2cb30b349e2b7f71cb50dfbe8b8_png );
LV_IMG_DECLARE( upload_wifioutline_1922d60a08324fb58b6286008444cc13_png );
LV_IMG_DECLARE( upload_fan0_4bd7cbea47ab4747a7ba1a8e654304d3_png );
LV_IMG_DECLARE( upload_fan2_4980bc102f3943d19cdcd71981ffdd64_png );
LV_IMG_DECLARE( upload_fan1_5f7d42737550431ebd8adb45b281d499_png );
LV_IMG_DECLARE( upload_fan3_84f063b5abbd4a2eb99f1df624dad654_png );
LV_IMG_DECLARE( upload_fanboost_09b958c807d44065830a9fbfb5e59bb8_png );
