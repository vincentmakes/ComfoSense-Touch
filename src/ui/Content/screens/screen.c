#include "../../GUI.h"



void GUI_initScreen__screen () {
    GUI_Screen__screen = lv_obj_create( NULL );
    lv_obj_remove_flag( GUI_Screen__screen, LV_OBJ_FLAG_SCROLLABLE );
     GUI_Panel__screen__panel = lv_obj_create( GUI_Screen__screen );
     lv_obj_remove_flag( GUI_Panel__screen__panel, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Panel__screen__panel, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Panel__screen__panel, -1, -197 );
     lv_obj_set_size( GUI_Panel__screen__panel, 465, 66 );

      GUI_Label__screen__time = lv_label_create( GUI_Panel__screen__panel );
      lv_obj_set_align( GUI_Label__screen__time, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Label__screen__time, -157, -13 );
      lv_obj_set_size( GUI_Label__screen__time, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Label__screen__date = lv_label_create( GUI_Panel__screen__panel );
      lv_obj_set_align( GUI_Label__screen__date, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Label__screen__date, -158, 13 );
      lv_obj_set_size( GUI_Label__screen__date, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Label__screen__filter = lv_label_create( GUI_Panel__screen__panel );
      lv_obj_set_align( GUI_Label__screen__filter, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Label__screen__filter, 151, 1 );
      lv_obj_set_size( GUI_Label__screen__filter, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Image__screen__image = lv_img_create( GUI_Panel__screen__panel );
      lv_obj_add_flag( GUI_Image__screen__image, LV_OBJ_FLAG_ADV_HITTEST );
      lv_obj_remove_flag( GUI_Image__screen__image, LV_OBJ_FLAG_SCROLLABLE );
      lv_obj_set_align( GUI_Image__screen__image, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Image__screen__image, 62, -3 );
      lv_obj_set_size( GUI_Image__screen__image, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Image__screen__wifi = lv_img_create( GUI_Panel__screen__panel );
      lv_obj_add_flag( GUI_Image__screen__wifi, LV_OBJ_FLAG_ADV_HITTEST );
      lv_obj_remove_flag( GUI_Image__screen__wifi, LV_OBJ_FLAG_SCROLLABLE );
      lv_obj_set_align( GUI_Image__screen__wifi, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Image__screen__wifi, -40, 19 );
      lv_obj_set_size( GUI_Image__screen__wifi, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Panel__screen__panel_1 = lv_obj_create( GUI_Screen__screen );
     lv_obj_remove_flag( GUI_Panel__screen__panel_1, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Panel__screen__panel_1, LV_ALIGN_BOTTOM_LEFT );
     lv_obj_set_pos( GUI_Panel__screen__panel_1, 9, -9 );
     lv_obj_set_size( GUI_Panel__screen__panel_1, 462, 77 );

      GUI_Button__screen__BSpeedMinus = lv_button_create( GUI_Panel__screen__panel_1 );
      lv_obj_set_align( GUI_Button__screen__BSpeedMinus, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Button__screen__BSpeedMinus, 20, 20 );
      lv_obj_set_size( GUI_Button__screen__BSpeedMinus, 150, 66 );

       GUI_Image__screen__minus = lv_img_create( GUI_Button__screen__BSpeedMinus );
       lv_obj_add_flag( GUI_Image__screen__minus, LV_OBJ_FLAG_ADV_HITTEST );
       lv_obj_remove_flag( GUI_Image__screen__minus, LV_OBJ_FLAG_SCROLLABLE );
       lv_obj_set_align( GUI_Image__screen__minus, LV_ALIGN_CENTER );
       lv_obj_set_size( GUI_Image__screen__minus, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Button__screen__BSpeedPlus = lv_button_create( GUI_Panel__screen__panel_1 );
      lv_obj_set_align( GUI_Button__screen__BSpeedPlus, LV_ALIGN_CENTER );
      lv_obj_set_pos( GUI_Button__screen__BSpeedPlus, 40, 40 );
      lv_obj_set_size( GUI_Button__screen__BSpeedPlus, 150, 65 );
      lv_obj_add_event_cb( GUI_Button__screen__BSpeedPlus, GUI_event__Button__screen__BSpeedPlus__Clicked, LV_EVENT_CLICKED, NULL );

       GUI_Image__screen__plus = lv_img_create( GUI_Button__screen__BSpeedPlus );
       lv_obj_add_flag( GUI_Image__screen__plus, LV_OBJ_FLAG_ADV_HITTEST );
       lv_obj_remove_flag( GUI_Image__screen__plus, LV_OBJ_FLAG_SCROLLABLE );
       lv_obj_set_align( GUI_Image__screen__plus, LV_ALIGN_CENTER );
       lv_obj_set_size( GUI_Image__screen__plus, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

      GUI_Button__screen__buttonspeedboost = lv_button_create( GUI_Panel__screen__panel_1 );
      lv_obj_set_align( GUI_Button__screen__buttonspeedboost, LV_ALIGN_CENTER );
      lv_obj_set_size( GUI_Button__screen__buttonspeedboost, 77, 65 );
      lv_obj_add_event_cb( GUI_Button__screen__buttonspeedboost, GUI_event__Button__screen__buttonspeedboost__Clicked, LV_EVENT_CLICKED, NULL );

       GUI_Label__screen__labelboost = lv_label_create( GUI_Button__screen__buttonspeedboost );
       lv_obj_set_align( GUI_Label__screen__labelboost, LV_ALIGN_CENTER );
       lv_obj_set_size( GUI_Label__screen__labelboost, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Label__screen__insideTemp = lv_label_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Label__screen__insideTemp, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Label__screen__insideTemp, 32, 5 );
     lv_obj_set_size( GUI_Label__screen__insideTemp, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Label__screen__outsideTemp = lv_label_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Label__screen__outsideTemp, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Label__screen__outsideTemp, -198, 2 );
     lv_obj_set_size( GUI_Label__screen__outsideTemp, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Label__screen__insideHum = lv_label_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Label__screen__insideHum, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Label__screen__insideHum, 34, 36 );
     lv_obj_set_size( GUI_Label__screen__insideHum, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Label__screen__outsideHum = lv_label_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Label__screen__outsideHum, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Label__screen__outsideHum, -197, 33 );
     lv_obj_set_size( GUI_Label__screen__outsideHum, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Label__screen__auto = lv_label_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Label__screen__auto, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Label__screen__auto, -25, 77 );
     lv_obj_set_size( GUI_Label__screen__auto, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Image__screen__fanspeed0 = lv_img_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Image__screen__fanspeed0, LV_OBJ_FLAG_ADV_HITTEST );
     lv_obj_remove_flag( GUI_Image__screen__fanspeed0, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Image__screen__fanspeed0, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Image__screen__fanspeed0, -17, 2 );
     lv_obj_set_size( GUI_Image__screen__fanspeed0, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Image__screen__fanspeed1 = lv_img_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Image__screen__fanspeed1, LV_OBJ_FLAG_ADV_HITTEST );
     lv_obj_remove_flag( GUI_Image__screen__fanspeed1, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Image__screen__fanspeed1, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Image__screen__fanspeed1, -17, 2 );
     lv_obj_set_size( GUI_Image__screen__fanspeed1, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Image__screen__fanspeed2 = lv_img_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Image__screen__fanspeed2, LV_OBJ_FLAG_ADV_HITTEST );
     lv_obj_remove_flag( GUI_Image__screen__fanspeed2, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Image__screen__fanspeed2, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Image__screen__fanspeed2, -17, 2 );
     lv_obj_set_size( GUI_Image__screen__fanspeed2, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Image__screen__fanspeed3 = lv_img_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Image__screen__fanspeed3, LV_OBJ_FLAG_ADV_HITTEST );
     lv_obj_remove_flag( GUI_Image__screen__fanspeed3, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Image__screen__fanspeed3, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Image__screen__fanspeed3, -17, 2 );
     lv_obj_set_size( GUI_Image__screen__fanspeed3, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Image__screen__fanspeedboost = lv_img_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Image__screen__fanspeedboost, LV_OBJ_FLAG_ADV_HITTEST );
     lv_obj_remove_flag( GUI_Image__screen__fanspeedboost, LV_OBJ_FLAG_SCROLLABLE );
     lv_obj_set_align( GUI_Image__screen__fanspeedboost, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Image__screen__fanspeedboost, -17, 2 );
     lv_obj_set_size( GUI_Image__screen__fanspeedboost, LV_SIZE_CONTENT, LV_SIZE_CONTENT );

     GUI_Dropdown__screen__modedropdown = lv_dropdown_create( GUI_Screen__screen );
     lv_obj_add_flag( GUI_Dropdown__screen__modedropdown, LV_OBJ_FLAG_SCROLL_ON_FOCUS );
     lv_dropdown_set_selected_highlight( GUI_Dropdown__screen__modedropdown, true );
     lv_obj_set_align( GUI_Dropdown__screen__modedropdown, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Dropdown__screen__modedropdown, 159, -131 );
     lv_obj_set_size( GUI_Dropdown__screen__modedropdown, 110, 39 );
     lv_obj_add_event_cb( GUI_Dropdown__screen__modedropdown, GUI_event__Dropdown__screen__modedropdown__Value_Changed, LV_EVENT_VALUE_CHANGED, NULL );

     GUI_Button__screen__centerfan = lv_button_create( GUI_Screen__screen );
     lv_obj_set_align( GUI_Button__screen__centerfan, LV_ALIGN_CENTER );
     lv_obj_set_pos( GUI_Button__screen__centerfan, -91, 11 );
     lv_obj_set_size( GUI_Button__screen__centerfan, 30, 30 );


    GUI_initScreenStyles__screen();
    GUI_initScreenTexts__screen();
}


void GUI_initScreenTexts__screen () {
      lv_label_set_text( GUI_Label__screen__time, "14:15" );
      lv_label_set_text( GUI_Label__screen__date, "2 Sept. 2025" );
      lv_label_set_text( GUI_Label__screen__filter, "Filter Change\n in 45 days" );
       lv_label_set_text( GUI_Label__screen__labelboost, "BOOST" );
     lv_label_set_text( GUI_Label__screen__insideTemp, "22°C" );
     lv_label_set_text( GUI_Label__screen__outsideTemp, "16°C" );
     lv_label_set_text( GUI_Label__screen__insideHum, "45%" );
     lv_label_set_text( GUI_Label__screen__outsideHum, "85%" );
     lv_label_set_text( GUI_Label__screen__auto, "Auto" );
     lv_dropdown_set_options( GUI_Dropdown__screen__modedropdown, "NORMAL\nHEATING\nCOOLING" );
}


void GUI_initScreenStyles__screen () {
    lv_obj_add_style( GUI_Screen__screen, &GUI_Style__class_IjzeXjGNl2hqzr__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Panel__screen__panel, &GUI_Style__class_YGYDXpOgt1PZ82__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_obj_add_style( GUI_Label__screen__time, &GUI_Style__class_Togbz3UaTq5x9x__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_obj_add_style( GUI_Label__screen__date, &GUI_Style__class_cGz9MAbjN4QZbT__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_obj_add_style( GUI_Label__screen__filter, &GUI_Style__class_x3c54mLQde9cTS__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_img_set_src( GUI_Image__screen__image, &upload_warningoutline_2343dccdd2da484cab2db0b1a6f4ab6b_png );

      lv_obj_add_style( GUI_Image__screen__image, &GUI_Style__class_2QBFIVRjDIrU4g__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_img_set_src( GUI_Image__screen__wifi, &upload_wifioutline_1922d60a08324fb58b6286008444cc13_png );

      lv_obj_add_style( GUI_Image__screen__wifi, &GUI_Style__class_5up8hpXSh7ZNGY__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Panel__screen__panel_1, &GUI_Style__class_1v6NR7CCKSOY1p__, LV_PART_MAIN | LV_STATE_DEFAULT );

      lv_obj_add_style( GUI_Button__screen__BSpeedMinus, &GUI_Style__class_ZQFNgZyvWmNGIo__, LV_PART_MAIN | LV_STATE_DEFAULT );

       lv_img_set_src( GUI_Image__screen__minus, &upload_minus_22b4150ed8454f4f8d0531614cfc727c_png );

      lv_obj_add_style( GUI_Button__screen__BSpeedPlus, &GUI_Style__class_qitjx2mC7B2BMR__, LV_PART_MAIN | LV_STATE_DEFAULT );

       lv_img_set_src( GUI_Image__screen__plus, &upload_plus_68d2c2cb30b349e2b7f71cb50dfbe8b8_png );

      lv_obj_add_style( GUI_Button__screen__buttonspeedboost, &GUI_Style__class_MeHq32ICIpTQi8__, LV_PART_MAIN | LV_STATE_DEFAULT );

       lv_obj_add_style( GUI_Label__screen__labelboost, &GUI_Style__class_fP8ERHUGv3DkcB__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Label__screen__insideTemp, &GUI_Style__class_LoaGBPa9UrG7CJ__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Label__screen__outsideTemp, &GUI_Style__class_xEEcgMbVy8vNcO__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Label__screen__insideHum, &GUI_Style__class_bAPqNI9bRId5Fl__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Label__screen__outsideHum, &GUI_Style__class_UtNSkp6cFAlBRt__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Label__screen__auto, &GUI_Style__class_8RAAmaM7qkaMFY__, LV_PART_MAIN | LV_STATE_DEFAULT );

     lv_img_set_src( GUI_Image__screen__fanspeed0, &upload_fan0_4bd7cbea47ab4747a7ba1a8e654304d3_png );

     lv_img_set_src( GUI_Image__screen__fanspeed1, &upload_fan1_5f7d42737550431ebd8adb45b281d499_png );

     lv_img_set_src( GUI_Image__screen__fanspeed2, &upload_fan2_4980bc102f3943d19cdcd71981ffdd64_png );

     lv_img_set_src( GUI_Image__screen__fanspeed3, &upload_fan3_84f063b5abbd4a2eb99f1df624dad654_png );

     lv_img_set_src( GUI_Image__screen__fanspeedboost, &upload_fanboost_09b958c807d44065830a9fbfb5e59bb8_png );

     lv_obj_add_style( GUI_Dropdown__screen__modedropdown, &GUI_Style__class_i65QyXalE1u3QM__, LV_PART_MAIN | LV_STATE_DEFAULT );
     lv_obj_add_style( GUI_Dropdown__screen__modedropdown, &GUI_Style__class_5qfkyxJclqQGgq__, LV_PART_MAIN | LV_STATE_DEFAULT );
     lv_obj_add_style( GUI_Dropdown__screen__modedropdown, &GUI_Style__class_e43nEdwxt3dT0L__, LV_PART_SELECTED | LV_STATE_DEFAULT );
     lv_obj_add_style( GUI_Dropdown__screen__modedropdown, &GUI_Style__class_kr8vNLKWRCqnOJ__, LV_PART_INDICATOR | LV_STATE_DEFAULT );

     lv_obj_add_style( GUI_Button__screen__centerfan, &GUI_Style__class_V1ohqLxZAM4F0a__, LV_PART_MAIN | LV_STATE_DEFAULT );

}


